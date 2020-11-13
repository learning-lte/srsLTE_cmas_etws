/*
 * Copyright 2013-2020 Software Radio Systems Limited
 *
 * This file is part of srsLTE.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include <cstdlib>
#include <inttypes.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string.h>
#include <sys/time.h>
#include <algorithm>

#include "srslte/common/log_filter.h"

namespace srslte {

bool log_filter::sib_recv    = false; 
bool log_filter::auth_rqst   = false;
bool log_filter::auth_succ   = false;
bool log_filter::is_fake     = false;
log_filter::Timer log_filter::my_timer = Timer();
log_filter::message_control log_filter::msg_control = message_control();


#define CHARS_FOR_HEX_DUMP(size)                                                                                       \
  (3 * size + size / 16 * 20) // 3 chars per byte, plus 20 per line for position and newline)

log_filter::log_filter() : log()
{
  do_tti      = false;
  time_src    = NULL;
  time_format = TIME;
  logger_h    = NULL;
}

log_filter::log_filter(std::string layer) : log()
{
  do_tti      = false;
  time_src    = NULL;
  time_format = TIME;
  init(layer, &def_logger_stdout, do_tti);
}

log_filter::log_filter(std::string layer, logger* logger_, bool tti) : log()
{
  do_tti      = false;
  time_src    = NULL;
  time_format = TIME;
  if (!logger_) {
    logger_ = &def_logger_stdout;
  }

  init(std::move(layer), logger_, tti);
}

void log_filter::init(std::string layer, logger* logger_, bool tti)
{
  // strip trailing white spaces
  size_t last_char_pos = layer.find_last_not_of(' ');
  if (last_char_pos != layer.size() - 1) {
    layer.erase(last_char_pos + 1, layer.size());
  }
  service_name = std::move(layer);
  logger_h     = logger_;
  do_tti       = tti;
}


void log_filter::all_log(srslte::LOG_LEVEL_ENUM level,
                         uint32_t               tti,
                         const char*            msg,
                         const uint8_t*         hex,
                         int                    size,
                         bool                   long_msg)
{
  char buffer_tti[16]  = {};
  char buffer_time[64] = {};

  if (logger_h) {
    logger::unique_log_str_t log_str = nullptr;

    if (long_msg || hex) {
      // For long messages, dynamically allocate a new log_str with enough size outside the pool.
      uint32_t log_str_msg_len = sizeof(buffer_tti) + sizeof(buffer_time) + 20 + strlen(msg) + CHARS_FOR_HEX_DUMP(size);
      log_str = logger::unique_log_str_t(new logger::log_str(nullptr, log_str_msg_len), logger::log_str_deleter());
    } else {
      log_str = logger_h->allocate_unique_log_str();
    }

    if (log_str) {
      now_time(buffer_time, sizeof(buffer_time)); // Time from buffer_time (format: Week Month Date H:M:S Year)
      for(int i = sizeof(buffer_time) - 1; i>=0; i--) // Removing last element from buffer_time (newline)
      {
        if(buffer_time[i] == '\n')
        {
          buffer_time[i] = '\0';
          break;
        }
      }
      if (do_tti) {
        get_tti_str(tti, buffer_tti, sizeof(buffer_tti));
      }

      snprintf(log_str->str(),
               log_str->get_buffer_size(),
               "%s [%-4s] %s %s%s%s%s%s",
               buffer_time,
               get_service_name().c_str(),
               log_level_text_short[level],
               do_tti ? buffer_tti : "",
               add_string_en ? add_string_val.c_str() : "",
               msg,
               msg[strlen(msg) - 1] != '\n' ? "\n" : "",
               (hex_limit > 0 && hex && size > 0) ? hex_string(hex, size).c_str() : "");
      std::string log_content = log_str->str();
      if (log_content.find("Authentication Request") != std::string::npos) std::cout << logger_h->get_filename() << std::endl;
      if (logger_h->get_filename().find("ue.log") != std::string::npos)
      {
          if (log_content.find("warningMessageSegment-r9") != std::string::npos && !sib_recv)
          {
            int pages;
            sib_recv = true;
            std::string message = "";
            log_content = find_sib_msg(log_content);
            sscanf(log_content.substr(0,2).c_str(), "%x", &pages);
            log_content = log_content.substr(2);
            int page_len = 83 * 2;
            for (int i = 0; i < pages; i++)
            {
                std::string message_tmp = log_content.substr(i * page_len, page_len);
                message += decode_sib_msg("/shell", message_tmp, page_len);
            }
            //recieve sib, timer start
            my_timer.start();
            std::cout << message;
            std::cout << sib_recv << std::endl;
            msg_control.set_msg(message);
          }
          if (sib_recv && my_timer.timer_enable() && !is_fake)
          {
            std::string log_content = log_str->str();
            float pass = my_timer.update();
            if (!auth_rqst)
            {
              //Check recieve Authentication Request?
              if (log_content.find("Authentication Request") != std::string::npos) 
              {
                auth_rqst = true;
                my_timer.reset();
              }
              else if (pass > 60.0) // After 60sec without Authentication Request
              {
                //this station is fake
                is_fake = true;
                fake_station_process(buffer_time);
              }
            }
            else if (auth_rqst && !auth_succ)
            {
              //Check Network authentication successful?
              if (log_content.find("Network authentication successful") != std::string::npos) 
              {
                auth_succ = true;
                my_timer.stop();
              }
              else if (pass > 10.0) // After 10sec without Network Successful
              {
                //this station is fake
                is_fake = true;
                fake_station_process(buffer_time);
              }        
            }
          }
          // printf("This is time : %s\n", buffer_time);
      }

      logger_h->log(std::move(log_str));
    } else {
      logger_h->log_char("Error in Log: Not enough buffers in pool\n");
    }
  }
}

void log_filter::console(const char* message, ...)
{
  char    args_msg[char_buff_size];
  va_list args;
  va_start(args, message);
  if (vsnprintf(args_msg, char_buff_size, message, args) > 0)
    printf("%s", args_msg); // Print directly to stdout
  fflush(stdout);
  va_end(args);
}

#define all_log_expand(log_level)                                                                                      \
  do {                                                                                                                 \
    if (level >= log_level) {                                                                                          \
      char    args_msg[char_buff_size];                                                                                \
      va_list args;                                                                                                    \
      va_start(args, message);                                                                                         \
      if (vsnprintf(args_msg, char_buff_size, message, args) > 0)                                                      \
        all_log(log_level, tti, args_msg);                                                                             \
      va_end(args);                                                                                                    \
    }                                                                                                                  \
  } while (0)

#define all_log_hex_expand(log_level)                                                                                  \
  do {                                                                                                                 \
    if (level >= log_level) {                                                                                          \
      char    args_msg[char_buff_size];                                                                                \
      va_list args;                                                                                                    \
      va_start(args, message);                                                                                         \
      if (vsnprintf(args_msg, char_buff_size, message, args) > 0)                                                      \
        all_log(log_level, tti, args_msg, hex, size);                                                                  \
      va_end(args);                                                                                                    \
    }                                                                                                                  \
  } while (0)

void log_filter::error(const char* message, ...)
{
  all_log_expand(LOG_LEVEL_ERROR);
}

void log_filter::warning(const char* message, ...)
{
  all_log_expand(LOG_LEVEL_WARNING);
}

void log_filter::info(const char* message, ...)
{
  all_log_expand(LOG_LEVEL_INFO);
}

void log_filter::info_long(const char* message, ...)
{
  if (level >= LOG_LEVEL_INFO) {
    char*   args_msg = NULL;
    va_list args;
    va_start(args, message);
    if (vasprintf(&args_msg, message, args) > 0)
      all_log(LOG_LEVEL_INFO, tti, args_msg, nullptr, strlen(args_msg), true);
    va_end(args);
    free(args_msg);
  }
}

void log_filter::debug(const char* message, ...)
{
  all_log_expand(LOG_LEVEL_DEBUG);
}

void log_filter::debug_long(const char* message, ...)
{
  if (level >= LOG_LEVEL_DEBUG) {
    char*   args_msg = NULL;
    va_list args;
    va_start(args, message);
    if (vasprintf(&args_msg, message, args) > 0)
      all_log(LOG_LEVEL_DEBUG, tti, args_msg, nullptr, strlen(args_msg), true);
    va_end(args);
    free(args_msg);
  }
}

void log_filter::error_hex(const uint8_t* hex, int size, const char* message, ...)
{
  all_log_hex_expand(LOG_LEVEL_ERROR);
}

void log_filter::warning_hex(const uint8_t* hex, int size, const char* message, ...)
{
  all_log_hex_expand(LOG_LEVEL_WARNING);
}

void log_filter::info_hex(const uint8_t* hex, int size, const char* message, ...)
{
  all_log_hex_expand(LOG_LEVEL_INFO);
}

void log_filter::debug_hex(const uint8_t* hex, int size, const char* message, ...)
{
  all_log_hex_expand(LOG_LEVEL_DEBUG);
}

void log_filter::set_time_src(time_itf* source, time_format_t format)
{
  this->time_src    = source;
  this->time_format = format;
}
void log_filter::get_tti_str(const uint32_t tti_, char* buffer, const uint32_t buffer_len)
{
  snprintf(buffer, buffer_len, "[%5d] ", tti_);
}

void log_filter::now_time(char* buffer, const uint32_t buffer_len)
{
  timeval rawtime  = {};
  tm      timeinfo = {};
  char    us[16];

  srslte_timestamp_t now;
  uint64_t           usec_epoch;

  if (buffer_len < 16) {
    fprintf(stderr, "Error buffer provided for time too small\n");
    return;
  }

  if (!time_src) {
    gettimeofday(&rawtime, nullptr);
    // gmtime_r(&rawtime.tv_sec, &timeinfo);
    localtime_r(&rawtime.tv_sec, &timeinfo);

    if (time_format == TIME) {
      // strftime(buffer, buffer_len, "%H:%M:%S.", &timeinfo);
      asctime_r(&timeinfo, buffer);
      // snprintf(us, 16, "%06ld", rawtime.tv_usec);
      uint32_t dest_len = (uint32_t)strlen(buffer);
      // strncat(buffer, us, buffer_len - dest_len - 1); // We don't need second detail(ms)
    } else {
      usec_epoch = rawtime.tv_sec * 1000000UL + rawtime.tv_usec;
      snprintf(buffer, buffer_len, "%" PRIu64, usec_epoch);
    }
  } else {
    now = time_src->get_time();

    if (time_format == TIME) {
      snprintf(buffer, buffer_len, "%ld:%06u", now.full_secs, (uint32_t)(now.frac_secs * 1e6));
    } else {
      usec_epoch = now.full_secs * 1000000UL + (uint64_t)(now.frac_secs * 1e6);
      snprintf(buffer, buffer_len, "%" PRIu64, usec_epoch);
    }
  }
}

std::string log_filter::hex_string(const uint8_t* hex, int size)
{
  std::stringstream ss;
  int               c = 0;

  ss << std::hex << std::setfill('0');
  if (hex_limit >= 0) {
    size = (size > hex_limit) ? hex_limit : size;
  }
  while (c < size) {
    ss << "             " << std::setw(4) << static_cast<unsigned>(c) << ": ";
    int tmp = (size - c < 16) ? size - c : 16;
    for (int i = 0; i < tmp; i++) {
      ss << std::setw(2) << static_cast<unsigned>(hex[c++]) << " ";
    }
    ss << "\n";
  }
  return ss.str();
}

std::string log_filter::find_sib_msg(std::string msg)
{
    int pos, len;
    pos = msg.find("warningMessageSegment-r9");
    pos = msg.find(": ", pos);
    pos = msg.find("\"", pos);   
    len = msg.find("\"", pos + 1) - pos - 1;
    msg = msg.substr(pos + 1, len);
    return msg;
}

std::string log_filter::decode_sib_msg(std::string root_path, std::string msg, int page_len)
{
    std::stringstream msg_text;
    int msg_len, flag;
    sscanf(msg.substr(page_len - 2).c_str(), "%x", &msg_len);
    std::fstream f("/shell/bytecode_decode", std::ios::out);
    f << msg.substr(0, msg_len * 2);
    f.close();
    flag = system((root_path + "/hex_to_str.sh").c_str());
    if (flag != -1)
    {
      f = std::fstream("/shell/text", std::ios::in);
      msg_text << f.rdbuf();
      f.close();
      return msg_text.str();
    }
    return "";
}
void log_filter::fake_station_process(char buffer_time[])
{
    // Reset to detect new log
    is_fake = sib_recv = auth_rqst = auth_succ = false;
    std::cout << "Fake Station Detected at time: " << buffer_time << std::endl;
    //show dectedet dialog in pc
    msg_control.show_dialog();
    my_timer.stop();
}

} // namespace srslte
