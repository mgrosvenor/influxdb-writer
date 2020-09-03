/*
 * Description: Influx-Writer is a small, simple, InfluxDB 2.0 client for C.
 * 				It enables applications to quickly log values to InfluxDB.
 * 				The library is for writing only, and is designed for speed and
 * 				simplicity. It is not designed or intended as a general purpose
 * 				InfluDB client library.
 *
 * 		 @file: influx-writer.h
 *  Created on: Aug 30, 2020
 *      Author: Matthew P. Grosvenor
 *      Contact <first initial><lastname>@gmail.com
 */

#ifndef INFLUXWRITER_H_
#define INFLUXWRITER_H_


#include <stdint.h>
#include <stdbool.h>


/**
 * @struct Nothing to see here. Opaque structure to hold internal state.
 */
struct ifwr_priv;

/**
 * @enum Error types for Influx-Writer
 */
typedef enum
{
	IFWR_ERR_NONE, 		/**< No error. Everything is good :-) */
	IFWR_ERR_NULLARG, 	/**< Argument supplied was null */
	IFWR_ERR_BADARGS, 	/**< Arguments supplied were bad */
	IFWR_ERR_SOCKET,	/**< Could not create socket */
	IFWR_ERR_HOSTNAME, 	/**< Could not resolve hostname */
	IFWR_ERR_CONNECT, 	/**< Could not connect or disconnected */
	IFWR_ERR_NOMEM,		/**< Could not allocate memory */
	IFWR_ERR_NOMEASURE, /**< No measurement name set. Can't send one! */
	IFWR_ERR_NOTAGS,	/**< Tried to send a measurement with no tagset */
	IFWR_ERR_NOFIELDS,	/**< Tried to send a measurement with no fields */
	IFWR_ERR_MSGTOOBIG, /**< Message exceeds max message size */
	IFWR_ERR_WRITEFAIL, /**< Failed to write to a file descriptor */
	IFWR_ERR_NOHEADER,  /**< Failed to write or create HTTP header */
	IFWR_ERR_NOCONTENT, /**< Failed to write or create HTTP content */
	IFWR_ERR_NOTSFMT,   /**< No timestamp format type set */
    IFWR_ERR_NOTIME,    /**< Could not get the time from the local Linux clock*/
    IFWR_ERR_BADHTTP,   /**< Bad HTTP response message */

	//*** !! Don't forget to update ifwr_err2str() and ifwr_errs_en[]. !! ***

	IFWR_ERR_UNKNOWN,	/**< An unknown error occurred */
} ifwr_err_e;


//Cap the maximum message size at 64K. Because it seems silly to leave it
//uncapped
#define IFWR_MAX_MSG 64 * 1024

typedef struct ifwr_priv
{
    ifwr_err_e last_err;
    int sockfd;
    char rx_buff[IFWR_MAX_MSG];
    char* default_measurement;
    char* default_tagset;
    int http_err_code;
    char* json_err_str;
} ifwr_priv_t;


/**
 * @struct Influx-Writer connection state. Supply parameters here to set up
 * 		and maintain the connection.
 */
typedef struct
{
	char* hostname; /**< Hostname string e.g "example.com" */
	int   port;		/**< Host port e.g. 9999 */
	char* org;		/**< Organization string (as supplied by InfluxDB */
	char* bucket;   /**< Bucket identifier (as supplied by InfluxDB */
	char* token;	/**< Authorization toke (as supplied by InfluxDB */

	struct ifwr_priv __private; //Don't touch my privates
} ifwr_conn_t;


/**
 * @brief Return the last error code
 *
 * @param[in] conn
 * 		InfluxDB connection State
 *
 */
ifwr_err_e ifwr_lasterr(ifwr_conn_t* conn );


/**
 * @brief Convert error code to string
 * TODO: Should take into account locale
 */
char* ifwr_err2str(ifwr_err_e err);


/**
 * @brief Convenience function to return the last error as a string
 */
char* ifwr_lasterr_str(ifwr_conn_t* conn );


/**
 * @brief Connect to the InfluxDB instance.
 *
 * @param[in,out]  conn
 * 		InfluxDB connection state with all public details supplied
 * @return 0 on success, -1 on failure. Failure may be:
 * 		IFWR_ERR_NULLARG an argument supplied is NULL (and should not be)
 * 		IFWR_ERR_BADARGS an argument supplied is unexpected
 * 		IFWR_ERR_CONNECT a connection could not be established
 */
int ifwr_connect(ifwr_conn_t* conn );


/**
 * @brief Set the default measurement name that we're working with.
 *
 * @param[in]	conn
 * 		InfluxDB connection state
 * @param[in]	measurement
 * 		Name of the measurement (see InfluxDB line protocol descrition)
 * @return
 * 		0 on success
 */
int ifwr_set_measurement(ifwr_conn_t* conn, char* measurement);


/**
 * @enum InfluxDB only supports a few predefined types, use this to help build
 * 		and format them.
 */
typedef enum
{
	IFWR_TYPE_UNKOWN = 0, 	/**< Type value is unknown or unset */
	IFWR_TYPE_INT,			/**< Type value is an integer (int64_t) */
	IFWR_TYPE_FLOAT,		/**< Type value is a float (double) */
	IFWR_TYPE_STRING,		/**< Type value is a string (char*) */
	IFWR_TYPE_BOOL,			/**< Type value is a boolean (bool) */
	IFWR_TYPE_STOP 		 	/**< Signals end of an array */
} ifwr_type_e;

/**
 * @union Storage type to hold the superset of all supported types
 */
typedef union
{
	int64_t i;
	double f;
	bool   b;
	char* s;
} ifwr_value_u;


/**
 * @struct Helper to build InfluxDB key/value pairs
 */
typedef struct
{
	char* key;
	ifwr_type_e type;
	ifwr_value_u value;
} ifwr_ktv_t;


/**
 * @brief Format a tagset into an InfluxDB Line Protocol string
 *
 * @param[in]	conn
 * 		InfluxDB connection state
 * @param[in] 	values
 * 		Array of Influx-writer type/values pairs
 * @param[in] len
 * 		maximum size of output
 * @param[in,out] buff
 * 		buffer to put string into
 *
 * @return Number of bytes written to the string, -1 on error
 */
int ifwr_fmt_tagset(ifwr_conn_t* conn, ifwr_ktv_t* values, int len, char* buff);


/**
 * @brief Set the default tagset for each measurement
 */
int ifwr_set_tagset(ifwr_conn_t* conn, char* tagset);


/**
 * @brief Format a feildset into an InfluxDB Line Protocol string
 *
 * @param[in]	conn
 * 		InfluxDB connection state
 * @param[in] 	values
 * 		Array of Influx-writer type/values pairs
 * @param[in] len
 * 		maximum size of output
 * @param[in,out] buff
 * 		buffer to put string into
 *
 * @return Number of bytes written to the string, -1 on error
 */
int ifwr_fmt_fieldset(ifwr_conn_t* conn, ifwr_ktv_t* values, int len, char* buff);

/**
 * @enum Set the measurement timestamp precision
 */
typedef enum
{
    IFWR_TS_UNDEF 	= 0,	/**< Undefined or not set */
    IFWR_TS_LOCAL,			/**< No timestamp supplied. Use local time */
    IFWR_TS_REMOTE, 		/**< No timesatmp supplied. Use InfluxDB time */
    IFWR_TS_SECS,			/**< Seconds only */
    IFWR_TS_MILLIS,			/**< Milliseconds */
    IFWR_TS_MICROS,			/**< Microseconds */
    IFWR_TS_NANOS,			/**< Nanoseconds */
} ifwr_fmt_e;


/**
 * @brief Send a measurement to InfluxDB
 *
 * @param[in]	conn
 * 		InfluxDB connection state
 * @param[in]	measure
 * 		measurement name, if NULL, use the default
 * @param[in] tags
 * 		InfluxDB Line protocol tag set, if NULL, the the default
 * @param[in] fields
 * 		InfluxDB Line protocol field set, cannot be NULL!
 * @param   fmt
 *      Timesatmp format
 * @param[in] ts
 * 		Timesatmp value in Unix epoch format with precision as above

 *
 * @return Number of bytes written to the string, -1 on error
 */
int ifwr_send(
		ifwr_conn_t* conn,
		const char* measurement,
		const ifwr_ktv_t* tags,
		const ifwr_ktv_t* fields,
        ifwr_fmt_e ts_fmt,
		int64_t ts_val);

/**
 * @brief Danger! Send whatever you provide to the InfluxDB endpoint.
 *
 * Use this only if you want to hand craft the line protocol input to influx
 *
 * @param[in]  conn
 * 		InfluxDB connection state
 * @param[in] format
 * 		C-style (printf) format string which will result in a valid InfluxDB
 * 		line protocol string once formatted
 * @param[in] ...
 * 		C-style (printf) arguments (for above)
 *
 * @return The number of bytes written
 */
__attribute__((__format__ (__printf__, 3, 4)))
int ifwr_write_raw(ifwr_conn_t* conn, const char* prec, const char* format, ... );

/**
 * @brief Get the result of a ifwr_write_raw(), or ifwr_send() functions.
 *
 * @param[in]  conn
 *      InfluxDB connection state
 *
 * @return 0 on success, -1 on failure. If failure, the http_err number can be
 */
int ifwr_response(ifwr_conn_t* conn);

/**
 * @brief Return the HTTP error code for the last write. Must be run after
 *      get_write_result() has been run
 *
 * @param[in]  conn
 *      InfluxDB connection state
 * @param[out] jason_msg
 *      Pointer to a string which will contain the JSON error message returned.
 *
 * @return the last HTTP error status number (typically ~400) for an error.
 */
int ifwr_http_err(ifwr_conn_t* conn, char** json_msg);



/**
 * @brief Close the connection to InfluxDB
 *
 * @param[in]	conn
 * 		InfluxDB connection state
 *
 */
void ifwr_close(ifwr_conn_t* conn);

#endif /* INFLUXWRITER_H_ */
