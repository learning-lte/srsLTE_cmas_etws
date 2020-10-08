# srsLTE_cmas_etws
Build and modify code from srsLTE(https://github.com/srsLTE/srsLTE)

My modify
* Add code in enb_cfg_parser.cc and enb_cfg_parser.h
  * Add function parse_sib10
  * Add function parse_sib12
    * CMAS messages can read specific files to get the message text.
