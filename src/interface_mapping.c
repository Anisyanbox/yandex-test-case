#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "cJSON.h"
#include "logger.h"
#include "interface_mapping.h"


/** pointer to table of interfaces. Table is fall in when 'intfmap_read_mapping_table' function is running
    and then it read only **/
struct if_map_t * interface_map = NULL;



void intfmap_read_mapping_table (void) {

  /** open mapping file **/
  FILE * map_file;
  map_file = fopen("interface_map.json", "r");
  if (map_file == NULL) {

    logger_error("Can't open input file 'interface_map.json'!\n");
    exit(EXIT_FAILURE);
  }

  /** find out file size and allocate buffer for its data**/
  struct stat statistics;
  if ( (stat("interface_map.json", &statistics)) < 0 ) {

    perror("stat(map_file, &statistics)");
    exit(EXIT_FAILURE);
  }

  /** set limit on file size **/
  if (statistics.st_size > MAX_INTERFACE_MAP_SIZE) {

    logger_error("too large mapping file!\n");
    exit(EXIT_FAILURE);
  }

  /** **/
  char * buffer = (char *)malloc(statistics.st_size + 1);

  /** read all data from file to buffer and close the file **/
  fread((void *)buffer, 1, statistics.st_size, map_file);
  fclose(map_file);
  buffer[statistics.st_size] = '\0';

  /** start JSON parsing **/
  cJSON * json = cJSON_Parse(buffer);
  if (json == NULL) {

    cJSON_Delete(json);
    free(buffer);
    logger_error("cjson_parse. Bad file format!\n");
    exit(EXIT_FAILURE);
  }

  int i;
  int num_interfaces = cJSON_GetArraySize(json);

  cJSON *ip2can = NULL;
  cJSON *port2can = NULL;
  cJSON *can2ip = NULL;
  cJSON *can2port = NULL;
  cJSON *can_id_interface = NULL;
  cJSON *can_id_protocol = NULL;
  cJSON *tmp_json = NULL;

  struct if_map_t *tmp_itnerface_map = NULL;

  for (i = 0; i < num_interfaces; ++i) {

    if (i == 0)  {
      interface_map = (struct if_map_t *)malloc(sizeof(struct if_map_t));
      tmp_itnerface_map = interface_map;
    }
    else {
      tmp_itnerface_map = tmp_itnerface_map->next = (struct if_map_t *)malloc(sizeof(struct if_map_t));
    }

    // distinguish one of the some objects
    tmp_json = cJSON_GetArrayItem(json, i);

    ip2can = cJSON_GetObjectItem(tmp_json, "ip-to-can");
    port2can = cJSON_GetObjectItem(tmp_json, "port-to-can");
    can2ip = cJSON_GetObjectItem(tmp_json, "can-to-ip");
    can2port = cJSON_GetObjectItem(tmp_json, "can-to-port");
    can_id_interface = cJSON_GetObjectItem(tmp_json, "can-interface-id");
    can_id_protocol = cJSON_GetObjectItem(tmp_json, "can-protocol-id");

    if ( (ip2can == NULL) && (port2can == NULL) && (can2ip == NULL) && \
         (can2port == NULL) && (can_id_interface == NULL) && (can_id_protocol) ) {

      cJSON_Delete(json);
      free(buffer);
      logger_error("wrong JSON\n");
      exit(EXIT_FAILURE);
    }

    // check valid ip address to send data to can
    if ( (cJSON_IsString(ip2can)) && (ip2can->valuestring != NULL) ) {

      tmp_itnerface_map->to_can.ip = ip2can->valuestring;
    }
    else {

      cJSON_Delete(json);
      free(buffer);
      logger_error("ip port must be in string format\n");
      exit(EXIT_FAILURE);
    }

    // check valid ip port to send data to can
    if (cJSON_IsNumber(port2can)) {

      tmp_itnerface_map->to_can.port = port2can->valueint;
    }
    else {

      cJSON_Delete(json);
      free(buffer);
      logger_error("port must be in integer format\n");
      exit(EXIT_FAILURE);
    }

    // check valid ip address to send data from can
    if ( (cJSON_IsString(can2ip)) && (can2ip->valuestring != NULL) ) {

      tmp_itnerface_map->from_can.ip = can2ip->valuestring;
    }
    else {

      cJSON_Delete(json);
      free(buffer);
      logger_error("ip port must be in string format\n");
      exit(EXIT_FAILURE);
    }

    // check valid ip port to sen data from can
    if (cJSON_IsNumber(can2port)) {

      tmp_itnerface_map->from_can.port = can2port->valueint;
    }
    else {

      cJSON_Delete(json);
      free(buffer);
      logger_error("port must be in integer format\n");
      exit(EXIT_FAILURE);
    }

    // check can-id-interface
    if ( (cJSON_IsString(can_id_interface)) && (can_id_interface->valuestring != NULL) ) {
      
      tmp_itnerface_map->to_can.can_id_interface = can_id_interface->valuestring;
      tmp_itnerface_map->from_can.can_id_interface = can_id_interface->valuestring;
    }
    else {

      cJSON_Delete(json);
      free(buffer);
      logger_error("can_id_interface must be in string format\n");
      exit(EXIT_FAILURE);    
    }

    // check can-id-protocol
    if (cJSON_IsNumber(can_id_protocol)) {

      tmp_itnerface_map->from_can.can_id_protocol = can_id_protocol->valueint;
      tmp_itnerface_map->to_can.can_id_protocol = can_id_protocol->valueint;
    }
    else {

      cJSON_Delete(json);
      free(buffer);
      logger_error("can_id_protocol must be in integer format\n");
      exit(EXIT_FAILURE);
    }
  }
  tmp_itnerface_map->next = NULL;

  /** end parsing **/
  free(buffer);

  /**  **/
  intfmap_delete_same_connection_from_map();

  /**  **/
  intfmap_print_diff_udp2can_conn();
  intfmap_print_diff_can2udp_conn();

  return;
}
/*******/




void intfmap_print_diff_udp2can_conn (void) {

  if (interface_map != NULL) {

    struct if_map_t *tmp_itnerface_map = interface_map;
    char str_port[10];
    
    #if (UDP2CAN_DEBUG)
      logger_empty("\n<<<*********\n");
    #endif
    
    while (true) {
      
      // to CAN connections
      sprintf(str_port,"%d",tmp_itnerface_map->to_can.port);
      strcpy(tmp_itnerface_map->to_can.ip_port_can_str_format, tmp_itnerface_map->to_can.ip);
      strcat(tmp_itnerface_map->to_can.ip_port_can_str_format,":");
      strcat(tmp_itnerface_map->to_can.ip_port_can_str_format, str_port);
      strcat(tmp_itnerface_map->to_can.ip_port_can_str_format, "-->");
      strcat(tmp_itnerface_map->to_can.ip_port_can_str_format, tmp_itnerface_map->to_can.can_id_interface);

      #if (UDP2CAN_DEBUG)
        logger_info("udp-->can    %s\n", tmp_itnerface_map->to_can.ip_port_can_str_format);
      #endif

      if (tmp_itnerface_map->next == NULL)
        break;

      tmp_itnerface_map = tmp_itnerface_map->next;
    }
    #if (UDP2CAN_DEBUG)
      logger_empty("*********>>>\n\n");
    #endif
  }

  return;
}
/*******/




void intfmap_print_diff_can2udp_conn (void) {

  if (interface_map != NULL) {

    struct if_map_t *tmp_itnerface_map = interface_map;
    char str_port[10];
    
    #if (UDP2CAN_DEBUG)
      logger_empty("<<<*********\n");
    #endif
    
    while (true) {
      
      // to UDP connections
      sprintf(str_port,"%d",tmp_itnerface_map->from_can.port);
      strcpy(tmp_itnerface_map->from_can.ip_port_can_str_format, tmp_itnerface_map->from_can.ip);
      strcat(tmp_itnerface_map->from_can.ip_port_can_str_format,":");
      strcat(tmp_itnerface_map->from_can.ip_port_can_str_format, str_port);
      strcat(tmp_itnerface_map->from_can.ip_port_can_str_format, "<--");
      strcat(tmp_itnerface_map->from_can.ip_port_can_str_format, tmp_itnerface_map->from_can.can_id_interface);

      #if (UDP2CAN_DEBUG)
      logger_info("can-->udp    %s\n", tmp_itnerface_map->from_can.ip_port_can_str_format);
      #endif

      if (tmp_itnerface_map->next == NULL)
        break;

      tmp_itnerface_map = tmp_itnerface_map->next;
    }
    #if (UDP2CAN_DEBUG)
      logger_empty("*********>>>\n\n");
    #endif
  }
  return;
}
/*******/







void intfmap_delete_same_connection_from_map (void) {

  struct if_map_t *tmp_tbl = NULL;
  struct if_map_t *tmp_tbl1 = NULL;
  struct if_map_t *tmp_tbl2 = NULL;

  /** calculate num of udp2can different connections and delete same connections **/
  tmp_tbl = interface_map;

  if (tmp_tbl->next != NULL) {

    while (tmp_tbl->next != NULL) {

      tmp_tbl1 = tmp_tbl;
      tmp_tbl2 = tmp_tbl->next;

      while (tmp_tbl2 != NULL) {

        // compare string ip-port-can from tbl1 vs ip-port-can tbl2
        // if strings are same - delete one same connection


        // update for next iteration
        tmp_tbl2 = tmp_tbl2->next;
      }
      
      // update for next iteration
      tmp_tbl = tmp_tbl->next;
    }
  }
  logger_todo("delete same connections from the interface map, if there are same interfaces in json-file\n");
  return;
}
/*******/








int intfmap_get_diff_udp2can_conn (void) {
  
  int num_udp2can_conn = 0;
  int tmp_flag = 0;

  struct if_map_t *tmp_tbl = NULL;
  struct if_map_t *tmp_tbl1 = NULL;
  struct if_map_t *tmp_tbl2 = NULL;

  /** calculate num of udp2can different connections and delete same connections **/
  tmp_tbl = interface_map;

  if (tmp_tbl->next == NULL) {

    num_udp2can_conn = 1;
  }
  else {

    num_udp2can_conn = 1;

    while (tmp_tbl->next != NULL) {

      tmp_tbl1 = tmp_tbl;
      tmp_tbl2 = tmp_tbl->next;

      while (tmp_tbl2 != NULL) {
        
        if ( (strcmp(tmp_tbl1->to_can.can_id_interface, tmp_tbl2->to_can.can_id_interface)) == 0 ) {
          
          tmp_flag = 1;
          tmp_tbl1->to_can.is_need_mutex = true;
          tmp_tbl2->to_can.is_need_mutex = true;

          #if (UDP2CAN_DEBUG)
            logger_debug("to %s mutex is needed\n", tmp_tbl1->to_can.can_id_interface);
          #endif
        }
        else {

          tmp_tbl2->to_can.is_need_mutex = false;

          #if (UDP2CAN_DEBUG)
            logger_debug("to %s mutex is not needed\n", tmp_tbl2->to_can.can_id_interface);
          #endif
        }
        // update for next iteration
        tmp_tbl2 = tmp_tbl2->next;

        if (tmp_flag == 0) {

          tmp_tbl1->to_can.is_need_mutex = false;

          #if (UDP2CAN_DEBUG)
            logger_debug("to %s mutex is not needed\n", tmp_tbl1->to_can.can_id_interface);
          #endif

        }
      }
      
      // update for next iteration
      tmp_tbl = tmp_tbl->next;
      ++num_udp2can_conn;
    }
  }

  return num_udp2can_conn;
}
/*******/


int intfmap_get_diff_can2udp_conn (void) {

  int num_can2udp_conn = 0;
  int tmp_flag = 0;

  struct if_map_t *tmp_tbl = NULL;
  struct if_map_t *tmp_tbl1 = NULL;
  struct if_map_t *tmp_tbl2 = NULL;

  char str_port[10] = {0};
  char tmp_str1[100] = {0};
  char tmp_str2[100] = {0};

  /** calculate num of udp2can different connections and delete same connections **/
  tmp_tbl = interface_map;

  if (tmp_tbl->next == NULL) {

    num_can2udp_conn = 1;
  }
  else {

    num_can2udp_conn = 1;

    while (tmp_tbl->next != NULL) {

      tmp_tbl1 = tmp_tbl;
      tmp_tbl2 = tmp_tbl->next;

      while (tmp_tbl2 != NULL) {

        // make string "ip:port"
        sprintf(str_port, "%d", tmp_tbl1->from_can.port);
        strcat(tmp_str1, tmp_tbl1->from_can.ip);
        strcat(tmp_str1, ":");
        strcat(tmp_str1, str_port);

        sprintf(str_port, "%d", tmp_tbl2->from_can.port);
        strcat(tmp_str2, tmp_tbl2->from_can.ip);
        strcat(tmp_str2, ":");
        strcat(tmp_str2, str_port);
        
        if ( (strcmp(tmp_str1, tmp_str2)) == 0 ) {
          
          tmp_flag = 1;
          tmp_tbl1->from_can.is_need_mutex = true;
          tmp_tbl2->from_can.is_need_mutex = true;

          #if (UDP2CAN_DEBUG)
            logger_debug("to %s mutex is needed\n", tmp_str1);
          #endif
        }
        else {

          tmp_tbl2->from_can.is_need_mutex = false;

          #if (UDP2CAN_DEBUG)
            logger_debug("to %s mutex is not needed\n", tmp_str2);
          #endif
        }
        // update for next iteration
        tmp_tbl2 = tmp_tbl2->next;

        if (tmp_flag == 0) {

          #if (UDP2CAN_DEBUG)
            logger_debug("to %s mutex is not needed\n", tmp_str1);
          #endif

          tmp_tbl1->from_can.is_need_mutex = false;
        }
      }
      
      // update for next iteration
      tmp_tbl = tmp_tbl->next;
      ++num_can2udp_conn;
    }
  }
  return num_can2udp_conn;
}
/*******/

