/*
 * test.c
 *
 *  Created on: Aug 30, 2020
 *      Author: m0110
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <string.h>

#include "debug.h"
#include "inih/ini.h"
#include "influx-writer.h"




//Set up the connection parameters
ifwr_conn_t conn = {
        .hostname = "localhost",
        .port     = 9999,
        .token    = "G6MoEcf4w_rvGJ_x-0x04e5Ax47CyWiiLYZgzhhuzro4hoIEd9w388WuhufPrp8l2eRmkJyS5I5I40sD6-as-w==",
        .org      = "75bf55139267dc4e",
        .bucket   = "adff3189f4259536"
};

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
static int handler(void* user, const char* section, const char* name,
                   const char* value)
{
    ifwr_conn_t* conn = (ifwr_conn_t*)user;

    if (MATCH("influxdb", "hostname")) {
        conn->hostname = strdup(value);
    }
    else if (MATCH("influxdb", "port")) {
        conn->port = atoi(value);
    }
    else if (MATCH("influxdb", "organization")) {
        conn->org = strdup(value);
    }
    else if (MATCH("influxdb", "bucket")) {
        conn->bucket = strdup(value);
    }
    else if (MATCH("influxdb", "token")) {
        conn->token = strdup(value);
    }
    else {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}



int main(int argc, char** argv)
{
    //Set up the connection parameters
    ifwr_conn_t conn = {0};

    if (ini_parse("example.ini", handler, &conn) < 0) {
        printf("Can't load 'test.ini'\n");
        return 1;
    }

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

    printf("Sending measurement at time = %i, temperature=%" PRIu64 ", pressure=%lf\n",
            time_now, fieldset[TEMP].value.i,  fieldset[PRES].value.f  );
    //Send the send the values to InfluxDB
    if(ifwr_send(&conn,measurement,tagset,fieldset,IFWR_TS_SECS,time_now) < 0){
        fprintf(stderr,"Could not send IFWR message. Error: %s\n",ifwr_lasterr_str(&conn));
        return -1;
    }

    //Check how influx DB responded
   if(ifwr_response(&conn)){
       //Uh ohh...
       char* json_msg = NULL;
       int http_error = ifwr_http_err(&conn, &json_msg);
       fprintf(stderr,"InfluxDB Failure with error code %i, and this message \"%s\"\n", http_error, json_msg);
       return -1;
   }



   //Now use the direct raw send API (which is kind of like printf)
    for(int i = 0; i < 100; i++){

        printf("Sending measurement at time = %i, temperature=%" PRIu64 ", pressure=%lf\n",
                time_now, fieldset[TEMP].value.i,  fieldset[PRES].value.f  );

        int ret = ifwr_write_raw(&conn,"s","weather,city=perth temp=%i,pressure=%lf %li", rand(), (double)rand(), time(NULL));
        if(ret < 0){
            fprintf(stderr,"Could not send IFWR message. Error: %s\n",ifwr_lasterr_str(&conn));
            return -1;
        }

        //Check how influx DB responded
       if(ifwr_response(&conn)){
           //Uh ohh...
           char* json_msg = NULL;
           int http_error = ifwr_http_err(&conn, &json_msg);
           fprintf(stderr,"InfluxDB Failure with error code %i, and this message \"%s\"\n", http_error, json_msg);
           return -1;
       }

       //Update the values
       fieldset[TEMP].value.i = rand();
       fieldset[PRES].value.f = rand();

    }

    // close the connectoin
    ifwr_close(&conn);
}
