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
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "srslte/common/log.h"
#include "srslte/common/logger.h"
#include "srslte/common/logger_stdout.h"
#include "srslte/phy/common/timestamp.h"

namespace srslte {

typedef std::string* str_ptr;

class log_filter : public srslte::log
{
public:
  static int     fake_detected_count;
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
    message_control() 
    { 
      warn_msg = ""; 
      cid = ""; 
      batch_count = 0;
      current_max = current_min = current_range = 0;
      sib12_recv = false;
    }
    void set_msg(std::string msg) 
    { 
      if (msg.length() > 0)
        warn_msg = msg;
      sib12_recv = true; 
    }
    void show_dialog()
    {
      std::string msg = warn_msg.substr(0, warn_msg.length() - 1);
      std::string cmd = "sudo /shell/warning.sh ";
      int show = system(cmd.c_str());
      sleep(3);
    }
    void set_cid(std::string cid_)
    {
      cid = cid_;
    }
    std::string  get_cid()
    {
      return cid;
    }
    void reset_snr_rsrp()
    {
      snr = 0;
      rsrp = 0;
      counts = 0;
    }
    void snr_rsrp_update(double snr_, double rsrp_)
    {
      snr += snr_;
      rsrp += rsrp_;
      counts++;
    }
    double get_snr_avg()
    {
      double snr_tmp = snr / (double)counts;
      return snr_tmp;
    }
    double get_rsrp_avg()
    {
      double rsrp_tmp = rsrp / (double)counts;
      if(rsrp_tmp > current_max){
        current_max = rsrp_tmp;
      }
      if(rsrp_tmp < current_min){
        current_min = rsrp_tmp;
      }
      current_range = current_max - current_min;
      return rsrp_tmp;
    }
    int get_counts()
    {
      return counts;
    }
    int get_batch()
    {
      return batch_count;
    }
    double get_batch_range()
    {
      return current_max - current_min;
    }
    double get_batch_max()
    {
      return current_max;
    }
    double get_batch_min()
    {
      return current_min;
    }
    void batch_update()
    {
      batch_count++;
    }
    void reset_batch()
    {
      batch_count = 0;
      current_range = 0;
      current_max = -1000000;
      current_min = 1000000;
    }
    bool get_sib12_recv()
    {
      return sib12_recv;
    }
  private:
    std::string warn_msg;
    std::string cid;
    double snr, rsrp;
    int counts;
    int batch_count;
    double  current_max, current_min, current_range;
    bool sib12_recv;
  };

  typedef enum { TIME, EPOCH } time_format_t;

  void set_time_src(time_itf* source, time_format_t format);

protected:
  logger* logger_h;
  bool    do_tti;
  static bool    sib_recv,sib2_recv, auth_rqst, auth_succ;
  static bool    detecte_dB_mode;
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
  std::string decode_sib_msg(std::string root_path, std::string msg, int page_len, std::ios_base::openmode mode);
  void        parse_sib(std::string log_content, char buffer_time[]);
  void        fake_station_process(char buffer_time[]);
  void        fake_detection(std::string log_content, char buffer_time[], bool new_sib2_recv);
};

} // namespace srslte

#endif // SRSLTE_LOG_FILTER_H
