/*
 * influxable.c
 *
 *  Created on: Aug 30, 2020
 *      Author: m0110
 */


#include <stdbool.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <libgen.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <inttypes.h>

#include "influx-writer.h"
#include "debug.h"



#define IFWR_SET_ERROR(errno) do { \
		conn->__private.last_err = errno; \
} while (0)




ifwr_err_e ifwr_lasterr(ifwr_conn_t* conn )
{
	return conn->__private.last_err;
}


static char* ifwr_errs_en[] = {
	"No error. Everything is good",
	"Require argument supplied is null",
	"Arguments supplied were bad",
	"Could not create socket",
	"Could not resovle hostname",
	"Could not connect or disconnected",
	"Could not allocate memory",
	"No measurement name set. Can't send one!",
	"Tried to send a measurement with no tagset",
	"Tried to send a measurement with no fields",
	"Message exceeds maximum message size",
	"Could not write to file descriptor",
	"Failed to write or create HTTP header",
    "Failed to write or create HTTP content",
    "No timestamp format type set",
    "Could not get the time from the local Linux clock",
    "Bad HTTP response message",

	"An unknown error occurred"
};


char* ifwr_err2str(ifwr_err_e err)
{
	switch(err){
		case IFWR_ERR_NONE: 		return ifwr_errs_en[0];
		case IFWR_ERR_NULLARG: 		return ifwr_errs_en[1];
		case IFWR_ERR_BADARGS: 		return ifwr_errs_en[2];
		case IFWR_ERR_SOCKET: 		return ifwr_errs_en[3];
		case IFWR_ERR_HOSTNAME:		return ifwr_errs_en[4];
		case IFWR_ERR_CONNECT: 		return ifwr_errs_en[5];
		case IFWR_ERR_NOMEM: 		return ifwr_errs_en[6];
		case IFWR_ERR_NOMEASURE: 	return ifwr_errs_en[7];
		case IFWR_ERR_NOTAGS: 		return ifwr_errs_en[8];
		case IFWR_ERR_NOFIELDS: 	return ifwr_errs_en[9];
		case IFWR_ERR_MSGTOOBIG:	return ifwr_errs_en[10];
		case IFWR_ERR_WRITEFAIL:    return ifwr_errs_en[11];
		case IFWR_ERR_NOHEADER:     return ifwr_errs_en[12];
        case IFWR_ERR_NOCONTENT:    return ifwr_errs_en[13];
        case IFWR_ERR_NOTSFMT:      return ifwr_errs_en[14];
        case IFWR_ERR_NOTIME:       return ifwr_errs_en[15];
        case IFWR_ERR_BADHTTP:      return ifwr_errs_en[16];

		case IFWR_ERR_UNKNOWN: return ifwr_errs_en[17];

		/* default: Deliberately no default case, let the compiler complain if
		 * we forget to add new error codes here!
		 */
	}

	return ifwr_errs_en[17];
}


char* ifwr_lasterr_str(ifwr_conn_t* conn )
{
	return(ifwr_err2str(ifwr_lasterr(conn)));
}


static int resolve_host(const char* hostname, struct in_addr* out)
{
    struct hostent* he = NULL;
    if ((he = gethostbyname(hostname)) == NULL)
    {
        // get the host info
        DBG("Error getting hostname: %s\n", hstrerror(h_errno));
        return -1;
    }

    struct in_addr** addr_list = (struct in_addr**) he->h_addr_list;

    for(int i = 0; addr_list[i]; i++)
    {
        //Return the first one;
        *out = *addr_list[0];

        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, out, str, INET_ADDRSTRLEN);

        DBG("Success! Host resolved. Returning address %s\n", str);
        return 0;
    }

    DBG("Error no IP addresses found!\n");
    return -1;
}



int ifwr_connect(ifwr_conn_t* conn )
{
	if(!conn){
		DBG("No connection supplied\n");
		IFWR_SET_ERROR(IFWR_ERR_NULLARG);
		return -1;
	}

	if(!conn->hostname){
		DBG("No hostname supplied\n");
		IFWR_SET_ERROR(IFWR_ERR_BADARGS);
		return -1;
	}

	if(conn->port < 0 || conn->port > 65536){
		DBG("Port number should be in the range [0..65536]\n");
		IFWR_SET_ERROR(IFWR_ERR_BADARGS);
		return -1;
	}

	if(!conn->org){
		DBG("No organization ID supplied\n");
		IFWR_SET_ERROR(IFWR_ERR_BADARGS);
		return -1;
	}

	if(!conn->bucket){
		DBG("No bucket ID supplied\n");
		IFWR_SET_ERROR(IFWR_ERR_BADARGS);
		return -1;
	}

	if(!conn->token){
		DBG("No authorization token supplied\n");
		IFWR_SET_ERROR(IFWR_ERR_BADARGS);
		return -1;
	}

	ifwr_priv_t* const priv = &conn->__private;

	priv->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (priv->sockfd == -1) {
		DBG("Socket creation failed...\n");
		IFWR_SET_ERROR(IFWR_ERR_SOCKET);
		return -1;
	}

	DBG("Socket successfully created..\n");

	struct sockaddr_in servaddr = {0};
	if(resolve_host(conn->hostname,&servaddr.sin_addr)){
		DBG("Error, could not resolve hostname %s\n", conn->hostname);
		IFWR_SET_ERROR(IFWR_ERR_HOSTNAME);
		return -1;
	}

	servaddr.sin_family         = AF_INET;
	servaddr.sin_port           = htons(conn->port);

	// connect the client socket to server socket
	if (connect(priv->sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
		DBG("connection with the server failed...\n");
		IFWR_SET_ERROR(IFWR_ERR_CONNECT);
		return -1;
	}

	DBG("Success! Connected to the server %s:%i..\n",
			conn->hostname,
			conn->port);

	return 0;
}

void ifwr_close(ifwr_conn_t* conn)
{
    if(!conn){
        DBG("No connection state supplied\n");
        IFWR_SET_ERROR(IFWR_ERR_NULLARG);
        return;
    }

    ifwr_priv_t* const priv = &conn->__private;
    close(priv->sockfd);

    DBG("Success! Closed the socket!\n");

}


int ifwr_set_measurement(ifwr_conn_t* conn, char* measurement)
{
	if(!conn){
		DBG("No connection supplied\n");
		IFWR_SET_ERROR(IFWR_ERR_NULLARG);
		return -1;
	}

	if(!measurement){
		DBG("No measurement name supplied\n");
		IFWR_SET_ERROR(IFWR_ERR_BADARGS);
		return -1;
	}

	ifwr_priv_t* const priv = &conn->__private;

	priv->default_measurement = measurement;

	DBG("Success! Set the default measuremt to \"%s\"\n", measurement);
	return 0;
}


static int ifwr_fmt_set(
		ifwr_conn_t* conn,
		ifwr_ktv_t* values,
		int len,
		char* buff,
		const char* settype //"tag"set or "field"set
)
{
	if(!conn){
		DBG("No connection state supplied\n");
		IFWR_SET_ERROR(IFWR_ERR_NULLARG);
		return -1;
	}

	if(!buff){
		DBG("No outut buffer supplied\n");
		IFWR_SET_ERROR(IFWR_ERR_BADARGS);
		return -1;
	}

	if(len < 0){
		DBG("No output buffer supplied\n");
		IFWR_SET_ERROR(IFWR_ERR_BADARGS);
		return -1;
	}

	if(len > IFWR_MAX_MSG){
		DBG("Buffer is too big\n");
		IFWR_SET_ERROR(IFWR_ERR_MSGTOOBIG);
		return -1;
	}

	int off = 0;
	for(ifwr_ktv_t* v = values;  v->type != IFWR_TYPE_STOP; v++){
		switch(v->type){
		case IFWR_TYPE_INT:
			off += snprintf(buff + off, len - off, "%s=%" PRIu64 "i,", v->key, v->value.i);
			continue;
		case IFWR_TYPE_FLOAT:
			off += snprintf(buff + off, len - off, "%s=%lf,", v->key, v->value.f);
			continue;
		case IFWR_TYPE_STRING:
			off += snprintf(buff + off, len - off, "%s=\"%s\",", v->key, v->value.s);
			continue;
		case IFWR_TYPE_BOOL:
			if(v->value.b)
				off += snprintf(buff + off, len - off, "%s=True,", v->key);
			else
				off += snprintf(buff + off, len - off, "%s=False,", v->key);
			continue;
		default:
			DBG("Unexpected type %i\n", v->type);
			IFWR_SET_ERROR(IFWR_ERR_UNKNOWN);
			return -1;
		}
	}

	/*
	 * As we exit this function, we have one too many trailing commas, remove
	 * and terminate the string properly.
	 */
	buff[off] = 0;
	off--;

	DBG("Successfully formatted %sset \"%s\"\n", settype, buff);
	return off;

}

int ifwr_fmt_tagset(ifwr_conn_t* conn, ifwr_ktv_t* values, int len, char* buff)
{
	return ifwr_fmt_set(conn, values, len, buff, "tag");
}


int ifwr_set_tagset(ifwr_conn_t* conn, char* tagset)
{
	if(!conn){
		DBG("No connection supplied\n");
		IFWR_SET_ERROR(IFWR_ERR_NULLARG);
		return -1;
	}

	if(!tagset){
		DBG("No tagset name supplied\n");
		IFWR_SET_ERROR(IFWR_ERR_BADARGS);
		return -1;
	}

	ifwr_priv_t* const priv = &conn->__private;

	priv->default_tagset = tagset;

	DBG("Success! Set the default tagset to \"%s\"\n", tagset);
	return 0;
}



int ifwr_fmt_fieldset(ifwr_conn_t* conn, ifwr_ktv_t* values, int len, char* buff)
{
	return ifwr_fmt_set(conn, values, len, buff, "field");
}

static int http_write(ifwr_conn_t* conn, const char* type, const char* buff, int len)
{
    ifwr_priv_t* const priv = &conn->__private;

    int written = 0;
    int attempts_remaing = 1000;
    DBG("Trying to write %i bytes of HTTP %s \"%s\"", len, type, buff);
    for(;len - written > 0 && attempts_remaing; attempts_remaing--){
        int ret = write(priv->sockfd, buff + written, len - written);
        if(ret < 0){
            ERR("Could not write %s. Error: %s", type, strerror(errno));
            IFWR_SET_ERROR(IFWR_ERR_WRITEFAIL);
            return -1;
        }
        written += ret;
    }

    if(written != len || attempts_remaing <= 0){
        DBG("Error could not write values! Tried 1000 times but failed\n");
        IFWR_SET_ERROR(IFWR_ERR_WRITEFAIL);
        return -1;
    }

    DBG("Success! Wrote %i bytes of HTTP %s\n", written);
    return written;
}


static int http_post_header(ifwr_conn_t* conn, int content_len, const char* prec)
{
    char buff[IFWR_MAX_MSG] = {0};

    int header_len = snprintf(buff, IFWR_MAX_MSG, "POST /api/v2/write?org=%s&bucket=%s&precision=%s HTTP/1.1\r\nHost: %s:%i\r\nContent-Length: %i\r\nContent-Encoding: identity\r\nContent-Type: text/plain\r\nAccept: application/json\r\nAuthorization: Token %s\r\nUser-Agent: exact-capture-influx 1.0\r\n\r\n",
        conn->org,
		conn->bucket,
		prec,
		conn->hostname,
		conn->port,
        content_len,
		conn->token);

   return http_write(conn, "Header", buff, header_len);
}


static int http_post_content(ifwr_conn_t* conn, const char* content, int content_len)
{
    return http_write(conn, "Content", content, content_len);
}



__attribute__((__format__ (__printf__, 3, 4)))
int ifwr_write_raw(ifwr_conn_t* conn, const char* prec, const char* format, ... )
{
    char content[IFWR_MAX_MSG] = {0};
    va_list args;
    va_start(args,format);
    int content_len = vsnprintf(content, IFWR_MAX_MSG,format, args);
    va_end(args);

    int sent_bytes = 0;
    int ret = http_post_header(conn, content_len, prec);
    if(ret < 0){
        ERR("Could not send HTTP header!");
        return -1;
    }
    sent_bytes += ret;

    ret = http_post_content(conn,content, content_len);
    if(ret < 0){
        IFWR_SET_ERROR(IFWR_ERR_NOCONTENT);
        ifwr_close(conn);
        FAT("HTTP Header sent, but content failed to send. Closing. Nothing useful to be done here!\n");
        return -1;
    }
    sent_bytes += ret;

    return sent_bytes;

}

static int ktv2str(char* buff, int buff_len, const ifwr_ktv_t* ktv )
{
    int written = 0;
    for(const ifwr_ktv_t* curr = ktv; curr->type != IFWR_TYPE_STOP; curr++){
        switch(curr->type){
            case IFWR_TYPE_STOP:
                //Impossible ? -- handled above
                FAT("Impossible situation has happend!?\n");
                break;

            case IFWR_TYPE_BOOL:
                if(curr->value.b){
                    written += snprintf(buff + written, buff_len - written,"%s=True,", curr->key );
                }
                else{
                    written += snprintf(buff + written, buff_len - written,"%s=False,", curr->key );
                }
                continue;

            case IFWR_TYPE_FLOAT:
                written += snprintf(buff + written, buff_len - written,"%s=%lf,", curr->key, curr->value.f );
                continue;

            case IFWR_TYPE_INT:
                written += snprintf(buff + written, buff_len - written,"%s=%" PRIu64 "i,", curr->key, curr->value.i );
                continue;

            case IFWR_TYPE_STRING:
                written += snprintf(buff + written, buff_len - written,"%s=\"%s\",", curr->key, curr->value.s );
                continue;

            case IFWR_TYPE_UNKOWN:
                ERR("Found a type of UNKOWN, was your KTV unitialised?\n");
                break;

        }
    }

    //Remove the tailing "," , replace with a null terminator
    if(buff_len - written > 0){
        buff[written -1] = 0;
    }

    return written;
}


//Take a look at the InfluxDB line protocol specification to see what this
//function is trying to build:
//https://v2.docs.influxdata.com/v2.0/reference/syntax/line-protocol/
int ifwr_send(
		ifwr_conn_t* conn,
		const char* measurement,
		const ifwr_ktv_t* tags,
		const ifwr_ktv_t* fields,
		ifwr_fmt_e ts_fmt,
		int64_t ts_val
		)
{
    if(!conn){
        DBG("No connection supplied\n");
        IFWR_SET_ERROR(IFWR_ERR_NULLARG);
        return -1;
    }

    ifwr_priv_t* const priv = &conn->__private;

    //Figure out the measurement name
    if(!measurement){
        if(!priv->default_measurement){
            IFWR_SET_ERROR(IFWR_ERR_NOMEASURE);
            ERR("No default measurement name is set\n");
            return -1;
        }

        measurement = priv->default_measurement;
    }
    DBG("Measurement set to \"%s\"\n", measurement);

    //Figure out the tags
    char* tags_str = NULL;
    if(!tags){
        if(!priv->default_tagset){
            IFWR_SET_ERROR(IFWR_ERR_NOTAGS);
            ERR("No default tagset is set\n");
            return -1;
        }
        tags_str = priv->default_tagset;
    }
    else{
        char tmp_tags[IFWR_MAX_MSG] = {0};
        ktv2str(tmp_tags, IFWR_MAX_MSG, tags);
        tags_str = tmp_tags;
    }
    DBG("Tags set to \"%s\"\n", tags_str);

    //Figure out the fields
    char fields_str[IFWR_MAX_MSG] = {0};
    if(!fields){
        IFWR_SET_ERROR(IFWR_ERR_NOFIELDS);
        ERR("No measurement fields supplied!\n");
        return -1;
    }

    ktv2str(fields_str, IFWR_MAX_MSG, fields);
    DBG("Fields set to \"%s\"\n", fields_str);


    //Figure out the timestamp
    char* prec = NULL;
    #define TS_LEN_MAX 64
    char ts_str_tmp[TS_LEN_MAX] = {0};
    char* ts_str = NULL;

    switch(ts_fmt){
        case IFWR_TS_UNDEF:
            IFWR_SET_ERROR(IFWR_ERR_NOTIME);
            ERR("No timestamp value set!\n");
            return -1;
        case IFWR_TS_LOCAL:{
            struct timespec now_ts = {0};
            if(clock_gettime(CLOCK_REALTIME, &now_ts) < 0){
                ERR("Could not get time! Error: %s", strerror(errno));
                IFWR_SET_ERROR(IFWR_ERR_NOTIME);
                return -1;
            }

            int64_t ns = now_ts.tv_sec * 1000 * 1000 * 1000 + now_ts.tv_nsec;

            prec = "ns";
            snprintf(ts_str_tmp,TS_LEN_MAX,"%" PRIu64,ns);
            ts_str = ts_str_tmp;
            break;
        }
        case IFWR_TS_REMOTE:
            prec = "ms"; //This seems sane. How are you going to get better than
                         //this over the network with a remote timestamp?
            ts_str = "";
            break;
        case IFWR_TS_NANOS:
            prec = "ns";
            //TODO - some sanity check that this is a sensible nanosecond value
            snprintf(ts_str_tmp,TS_LEN_MAX,"%" PRIu64 ,ts_val);
            ts_str = ts_str_tmp;
            break;
        case IFWR_TS_MICROS:
            prec = "us";
            //TODO - some sanity check that this is a sensible microsecond value
            snprintf(ts_str_tmp,TS_LEN_MAX,"%" PRIu64,ts_val);
            ts_str = ts_str_tmp;
            break;
        case IFWR_TS_MILLIS:
            prec = "ms";
            //TODO - some sanity check that this is a sensible milliscecond value
            snprintf(ts_str_tmp,TS_LEN_MAX,"%" PRIu64,ts_val);
            ts_str = ts_str_tmp;
            break;
        case IFWR_TS_SECS:
            prec = "s";
            //TODO - some sanity check that this is a sensible seconds value
            snprintf(ts_str_tmp,TS_LEN_MAX,"%" PRIu64,ts_val);
            ts_str = ts_str_tmp;
            break;

         /*default: no default case intentional. Let the compiler pick up if
          * I've forgotten a value.
          * */
    }
    DBG("Timestamp precision is \"%s\"\n", prec);
    DBG("Timestamp string is \"%s\"\n ", ts_str_tmp);


    //At this point we have strings for everything, just need to format it
    return ifwr_write_raw(conn, prec, "%s,%s %s %s\r\n",
            measurement,
            tags_str,
            fields_str,
            ts_str_tmp);

}


int ifwr_response(ifwr_conn_t* conn)
{
    if(!conn){
          DBG("No connection supplied\n");
          IFWR_SET_ERROR(IFWR_ERR_NULLARG);
          return -1;
      }

    ifwr_priv_t* const priv = &conn->__private;


    read(priv->sockfd, priv->rx_buff, IFWR_MAX_MSG);

    char* token = strtok(priv->rx_buff,"\r\n");

    //Check if we've got a correct HTTP header
    if(memcmp(token,"HTTP/1.1 ",9) != 0){
        ERR("Could not interpret \"%s\" as an HTTP header response\n", token);
        IFWR_SET_ERROR(IFWR_ERR_BADHTTP);
        return -1;

    }

    //Grab the response code
    char err[4] = {0};
    char* end;
    memcpy(err,token + 9,3);
    long http_err_code = strtol(err,&end,10);
    if(end == err){
        ERR("No error code found in HTTP header response \"%s\"\n", token);
        IFWR_SET_ERROR(IFWR_ERR_BADHTTP);
        return -1;
    }

    priv->http_err_code = http_err_code;
    if(http_err_code >= 200 && http_err_code < 300 ){
        DBG("Success with HTTP response code %li\n", http_err_code);
        priv->json_err_str = NULL;
        return 0;
    }

    token = strtok(NULL,"\r\n");
    while(token != NULL){
        if(token[0] == '{'){ //HACK! Assume the error line is JSON and starts with "{"
            DBG("Failure with HTTP response code %li, message \"%s\"\n", http_err_code, token );
            priv->json_err_str = token;
            return -1;
        }
        token = strtok(NULL,"\r\n");
    }

    //If we get here, we never found the Jason line!
    DBG("Could not find JSON response. Bad HTTP message?\n");
    IFWR_SET_ERROR(IFWR_ERR_BADHTTP);
    return -1;
}

int ifwr_http_err(ifwr_conn_t* conn, char** json_msg)
{
    if(!conn){
          DBG("No connection supplied\n");
          IFWR_SET_ERROR(IFWR_ERR_NULLARG);
          return -1;
      }

    ifwr_priv_t* const priv = &conn->__private;

    *json_msg = priv->json_err_str;
    return priv->http_err_code;
}



