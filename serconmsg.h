#ifndef __SERCONMSG_H__
#define __SERCONMSG_H__

/* All data structure assume Little Endian byte ordering */

struct serconmsg_t {
  unsigned char  id;     /* type of message */
  unsigned char  crc;    /* hash of message content for integrity check */
  unsigned short len;    /* length of data */
  unsigned int   serial; /* serial number of message */
};

/* 
 * Acknoledge message
 * used for messages :
 *  SERCONMSG_ACK
 */
struct serconmsg_ack_t {
  struct serconmsg_t head;
  unsigned int serial;     /* serial number of message to ack */
  unsigned int status;     /* status 0 means OK, otherwise error code */
  unsigned int reqid;      /* output request identifier or 0 if irrelevant */
  char data[1];            /* extra data - might be empty*/
};

/* 
 * data exchange
 * used for messages :
 *  SERCONMSG_RAW
 *  SERCONMSG_FILE_DATA
 *  SERCONMSG_TCP_DATA
 *  SERCONMSG_SETENV
 */
struct serconmsg_data_t {
  struct serconmsg_t head;
  unsigned int reqid;       /* request identifier (i.e. recipient) */
  char data[1];             /* data to transfer length defined in header */
};

/* 
 * request to create a file on remote system 
 * used for messages :
 *  SERCONMSG_SEND_FILE
 *  SERCONMSG_RECV_FILE
 *  SERCONMSG_FORK
 */
struct serconmsg_file_t {
  struct serconmsg_t head;
  char path[1];              /* path of file to create */
};

/* 
 * request to start TCP forwarding
 * used for messages :
 *  SERCONMSG_SEND_FILE
 *  SERCONMSG_RECV_FILE
 */
struct serconmsg_tcp_t {
  struct serconmsg_t head;
  unsigned short localhostlen;   /* define host:port to join from machine */
  unsigned short localport;      /* sending the message. */
  unsigned short remotehostlen;  /* define host:port to join from machine */
  unsigned short remoteport;     /* receiving the message */
  char direction;                /* transfer direction : */
                                 /*   FORWARD means message sender listens */
                                 /*   REVERSE means message receiver listens */
  char data[1];
};

/* 
 * request to change TTY size on receiver machine
 * used for messages :
 *  SERCONMSG_TTY_SIZE
 */
struct serconmsg_ttysize_t {
  struct serconmsg_t head;
  short width;               /* desired width */
  short height;              /* desired height */
};


/*
 * Data structure used to report statistics about a "stream"
 * of data on the connection.
 */
struct request_stats_t {
  int len;                      /* len of record */
  unsigned int reqid;           /* request identifier */
  unsigned int elapsed;         /* number of seconds request is running */
  unsigned int sndbytes;        /* total number of bytes sent */
  unsigned int recbytes;        /* total number of bytes received */
  double sndrate;               /* number of bytes sent per second */
  double recrate;               /* number of bytes received per second */
  char desc[1];                 /* request info - null terminated string */
};

/* 
 * Statistics about connection
 * used for messages :
 *  SERCONMSG_STATS
 */
struct serconmsg_stats {
  struct serconmsg_t head;
  unsigned int elapsed;         /* number of seconds connection is running */
  unsigned int sndbytes;        /* total number of bytes sent */
  unsigned int recbytes;        /* total number of bytes received */
  double sndrate;               /* number of bytes sent per second */
  double recrate;               /* number of bytes received per second */
  unsigned int reqs;            /* total number of request handled so far */
  unsigned int activereqs;      /* number of active requests */
};

enum serconmsg_e
  {
   SERCONMSG_ACK = 1,             /* generic ack of message */
   SERCONMSG_RAW,                 /* raw data tranmission */
   SERCONMSG_SEND_FILE,           /* request to send a file over connection */
   SERCONMSG_RECV_FILE,           /* ask to receive a file */
   SERCONMSG_RECV_FILE_START,     /* sent to ask for first block */
   SERCONMSG_FILE_DATA,           /* file data */
   SERCONMSG_TCP_FORWARD,         /* TCP forwarding request */
   SERCONMSG_TCP_REVERSE,         /* TCP reverse tunnel request */
   SERCONMSG_X11_FORWARD,         /* X11 forwarding request */
   SERCONMSG_TCP_DATA,            /* TCP data used for TCP both ways and X11*/
   SERCONMSG_TTY_SIZE,            /* request for TTY size change */
   SERCONMSG_SETENV,              /* sets environnement variable on remote system
                                   * must be called before forking process */
   SERCONMSG_FORK,                /* request process creation */
   SERCONMSG_STATS,               /* reports statistics */
   SERCONMSG_BYE,                 /* tell receiver that connection is over */
   SERCONMSG_MAX
  };


struct sercon_req_t {
  struct sercon_req_t *next;    /* next in linked list */
  unsigned int reqid;           /* request identifier */
  unsigned int elapsed;         /* number of seconds request is running */
  unsigned int sndbytes;        /* total number of bytes sent */
  unsigned int recbytes;        /* total number of bytes received */
  int state;                    /* state of request */
  char *info;                   /* extra info */
};

struct sercon_req_file_t {
  struct sercon_req_t head;
  FILE *f;                      /* opened file for reading or writing */
  short flags;                  /* flags indicating direction and mode */
};

struct sercon_req_tcp_t {
  struct sercon_req_t head;     
  int socket;                   /* client or server socket */
  int type;                     /* type of TCP tunnel */
};

struct sercon_awm_t {
};

/*
 * Data structure describing a connection
 */
struct sercon_cnx_t {
  int fd;                         /* file descriptor */
  int reqs;                       /* number of active requests */
  int nextreqid;                  /* next request identifier */
  int nextserial;                 /* next serial number to use */
  struct sercon_req_t reqlist;    /* list of requests */
  struct sercon_awm_t awmlist;    /* list of messages currently awaiting ack */
};



#endif
