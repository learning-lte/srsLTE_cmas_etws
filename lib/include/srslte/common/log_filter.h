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

/******************************************************************************
 * File:        log_filter.h
 * Description: Log filter for a specific layer or element.
 *              Performs filtering based on log level, generates
 *              timestamped log strings and passes them to the
 *              common logger object.
 *****************************************************************************/

#ifndef SRSLTE_LOG_FILTER_H
#define SRSLTE_LOG_FILTER_H

#include <stdarg.h>
#include <string>

#include "srslte/common/log.h"
#include "srslte/common/logger.h"
#include "srslte/common/logger_stdout.h"
#include "srslte/phy/common/timestamp.h"

namespace srslte {

typedef std::string* str_ptr;

class log_filter : public srslte::log
{
public:
  log_filter();
  log_filter(std::string layer);
  log_filter(std::string layer, logger* logger_, bool tti = false);

  void init(std::string layer, logger* logger_, bool tti = false);

  void console(const char* message, ...) __attribute__((format(printf, 2, 3)));
  void error(const char* message, ...) __attribute__((format(printf, 2, 3)));
  void warning(const char* message, ...) __attribute__((format(printf, 2, 3)));
  void info(const char* message, ...) __attribute__((format(printf, 2, 3)));
  void info_long(const char* message, ...) __attribute__((format(printf, 2, 3)));
  void debug(const char* message, ...) __attribute__((format(printf, 2, 3)));
  void debug_long(const char* message, ...) __attribute__((format(printf, 2, 3)));

  void error_hex(const uint8_t* hex, int size, const char* message, ...) __attribute__((format(printf, 4, 5)));
  void warning_hex(const uint8_t* hex, int size, const char* message, ...) __attribute__((format(printf, 4, 5)));
  void info_hex(const uint8_t* hex, int size, const char* message, ...) __attribute__((format(printf, 4, 5)));
  void debug_hex(const uint8_t* hex, int size, const char* message, ...) __attribute__((format(printf, 4, 5)));

  srslte::LOG_LEVEL_ENUM get_level(std::string l);

  class time_itf
  {
  public:
    virtual srslte_timestamp_t get_time() = 0;
  };
  class Timer
  {
  public:
    Timer()
    {
      enable = false;
      beg = 0;
      end = 0;
      pass = 0;
    }
    void start()
    {
      enable = true;
      beg = clock();
    }
    float update()
    {
      end = clock();
      pass = (float)(end - beg) / CLOCKS_PER_SEC;
      return pass * enable;
    }
    void stop()
    {
      enable = false;
      beg = end = pass = 0;
    }
    void reset()
    {
      enable = true;
      beg = end = pass = 0;
      beg = clock();
    }
    bool timer_enable()
    {
      return enable;
    }
  private:
    bool enable;
    float pass;
    clock_t beg, end;
  };

  class message_control
  {
  public:
    message_control() { warn_msg = ""; cid = ""; }
    void set_msg(std::string msg) { warn_msg = msg; }
    void show_dialog()
    {
      std::string msg = warn_msg.substr(0, warn_msg.length() - 1);
      std::string cmd = "sudo /shell/warning.sh " + cid  + " \"" +msg +  "\"";
      int show = system(cmd.c_str());      
    }
    void set_cid(std::string cid_)
    {
      cid = cid_;
    }
    std::string  get_cid()
    {
      return cid;
    }
  private:
    std::string warn_msg;
    std::string cid;
  };

  typedef enum { TIME, EPOCH } time_format_t;

  void set_time_src(time_itf* source, time_format_t format);

protected:
  logger* logger_h;
  bool    do_tti;
  static bool    sib_recv, auth_rqst, auth_succ, is_fake;
  static Timer my_timer;
  static message_control msg_control;

  static const int char_buff_size = logger::preallocated_log_str_size - 64 * 3;

  time_itf*     time_src;
  time_format_t time_format;

  logger_stdout def_logger_stdout;
  void        all_log(srslte::LOG_LEVEL_ENUM level,
                      uint32_t               tti,
                      const char*            msg,
                      const uint8_t*         hex      = nullptr,
                      int                    size     = 0,
                      bool                   long_msg = false);
  void        now_time(char* buffer, const uint32_t buffer_len);
  void        get_tti_str(const uint32_t tti_, char* buffer, const uint32_t buffer_len);
  std::string hex_string(const uint8_t* hex, int size);
  std::string find_sib_msg(std::string msg);
  std::string decode_sib_msg(std::string root_path, std::string msg, int page_len);
  void        fake_station_process(char buffer_time[]);
};

} // namespace srslte

#endif // SRSLTE_LOG_FILTER_H
