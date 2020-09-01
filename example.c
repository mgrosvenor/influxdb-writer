/*
 * test.c
 *
 *  Created on: Aug 30, 2020
 *      Author: m0110
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "influx-writer.h"



int main()
{

    //Set up the connection parameters
    ifwr_conn_t conn = {
            .hostname = "example.com",
            .port     = 9999,
            .token    = "IAzEi9ETX9qSHH58a97ObTOW32JOQI19ntqgZGb6EIfqg==",
            .org      = "1cbb42",
            .bucket   = "25b574661"
    };

    //Connect
    if(ifwr_connect(&conn)){
        fprintf(stderr,"Could not connect to IFDB %s\n",ifwr_lasterr_str(&conn));
        return -1;
    }

    //A sample measurement using the helper structures
    char* measurement = "weather";

    ifwr_ktv_t tagset[] = {
        { .type=IFWR_TYPE_STRING, .key = "city", .value.s = "Perth" },
        { .type=IFWR_TYPE_STOP }
    };

    const int TEMP = 0;
    const int PRES = 1;
    ifwr_ktv_t fieldset[] = {
        { .type=IFWR_TYPE_INT,   .key = "temperature", .value.i = 0   },
        { .type=IFWR_TYPE_FLOAT, .key = "pressure",    .value.f = 0.0 },
        { .type=IFWR_TYPE_STOP }
    };

    fieldset[TEMP].value.i = rand();
    fieldset[PRES].value.f = rand();

    int time_now = time(NULL);

    //Send the send the values to InfluxDB
    if(ifwr_send(&conn,measurement,tagset,fieldset,IFWR_TS_SECS,time_now)){
        fprintf(stderr,"Could not send IFWR message. Error: %s\n",ifwr_lasterr_str(&conn));
        return -1;
    }

    //Check how influx DB responded
   if(get_write_result(&conn)){
       //Uh ohh...
       char* json_msg = NULL;
       int http_error = get_http_err(&conn, &json_msg);
       fprintf(stderr,"InfluxDB Failure with error code %i, and this message \"%s\"\n", http_error, json_msg);
       return -1;
   }


   //Now use the direct raw send API (which is kind of like printf)
    char line[1024] = {};
    for(int i = 0; i < 100; i++){
        int ret = ifwr_write_raw(&conn,"weather,city=perth temp=%lii,pressure=%lf %i", rand(), rand(), time(NULL));
        if(ret < 0){
            fprintf(stderr,"Could not send IFWR message. Error: %s\n",ifwr_lasterr_str(&conn));
            return -1;
        }

        //Check how influx DB responded
       if(get_write_result(&conn)){
           //Uh ohh...
           char* json_msg = NULL;
           int http_error = get_http_err(&conn, &json_msg);
           fprintf(stderr,"InfluxDB Failure with error code %i, and this message \"%s\"\n", http_error, json_msg);
           return -1;
       }
    }

    // close the connectoin
    ifwr_close(&conn);
}
