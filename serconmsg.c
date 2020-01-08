#include "serconmsg.h"


/*
 * sercon_alloc_cnx --
 *
 *   Allocates a connection structure
 */
struct sercon_cnx_t *sercon_alloc_cnx()
{
  struct sercon_cnx_t *cnx;
  cnx = (struct sercon_cnx_t*) malloc(sizeof(struct sercon_cnx_t));
  if ( cnx == NULL ) {
    sercon_panic("memory allocation error");    
  }
  memset( cnx, 0, sizeof(cnx) );
  cnx->fd = -1;
  return NULL;
}

/*
 * sercon_set_tty --
 *
 *
 */
int sercon_set_tty( cnx, ttyfd )
     struct sercon_cnx_t *cnx;
     int ttyfd;
{
  sercon->fd = ttyfd;
}

/*
 * sercon_crc_compute --
 *
 *    Compute the crc of a message
 */
unsigned char sercon_crc_compute( msg )
     struct sercon_msg_t *msg;
{
  unsigned char crc = 0;
  unsigned char *bmsg = (unsigned char*) msg;
  int i;
  for(i = 0; i < msg->len; ++i ) {
    if ( bmsg + i == (unsigned char*)&(msg->crc) ) {
      continue;
    }
    crc = (crc << 3) | bmsg[i];
  }
  return crc;
}

/*
 * sercon_crc_check --
 *
 *   Verify the CRC of supplied message
 */
int sercon_crc_check(msg)
     struct sercon_msg_t *msg;
{
  unsigned char crc;
  crc = sercon_crc_compute(msg);
  return (crc == msg->crc) ? SERCON_OK : SERCON_ERR_CRC;
}

/* 
 *  sercon_require_req --
 *
 *     Retreives the request associated to a message.
 * 
 *  Results:
 *     NULL or a request pointer.
 */
struct sercon_req_t *sercon_find_req( cnx, msg )
     struct sercon_cnx_t *cnx;
     struct sercon_msg_t *msg;    /* incoming message to dispatch */
{
  struct sercon_req_t *req = NULL;
  if ( msg->reqid <= 0 ) {
    return NULL;
  }
  for ( req = sercon_first_req(cnx); req; req = sercon_next_req(cnx, req)) {
    if ( req->reqid == msg->reqid ) {
      break;
    }
  }
  return req;
}

/* 
 *  sercon_require_req --
 *  
 *    Checks if request is associated to the message.
 *    If a request id is associated to the message
 *    it is checked against the request list of cnx
 * 
 *  Returns:
 *    SERCON_OK (=0) on success
 *    SERCON_ERR_NO_REQUEST on error
 *
 *  Side effects:
 *    None
 */
int sercon_require_req( cnx, msg )
{
  return (sercon_find_req( cnx, msg) != NULL) ? SERCON_OK : SERCON_ERR_NO_REQUEST;
}

/*
 * sercon_dispatch --
 *
 *   This function dispatch an incoming message to the right handler
 */
int sercon_dispatch( cnx, msg )
     struct sercon_cnx_t *cnx;
     struct sercon_msg_t *msg;    /* incoming message to dispatch */
{
  struct sercon_req_t *req = NULL;
  int crcok;

  /* check message integrity */
  crcok = sercon_crc_check(msg);
  if ( SERCON_OK != crcok )  {
    return sercon_error( cnx, crcok, msg );
  }
  
  /* update stats */
  /* stats are updated after CRC check because if CRC is bad
   * it can mean that msg->len is irrelevant */
  cnx->recbytes += msg->len;

  /* find associated request */
  req = sercom_find_req( cnx, msg );
  if ( req != NULL ) {
    req->recbytes += msg->len;
  }
  
  switch( msg->id ) {
  case SERCONMSG_ACK:
    return sercon_handle_ack( cnx, msg );
    break;
  case SERCONMSG_SEND_FILE:
    return sercon_handle_send_file( cnx, msg );
    break;
  case SERCONMSG_RECV_FILE:
    return sercon_handle_recv_file( cnx, msg );
    break;
  case SERCONMSG_RECV_FILE_START:
    return sercon_require_req( cnx, msg )
      || sercon_handle_recv_file_start( cnx, msg );
    break;
  case SERCONMSG_FILE_DATA:
    return sercon_require_req( cnx, msg )
      || sercon_handle_file_data( cnx, msg );
    break;
  case SERCONMSG_TCP_FORWARD:
  case SERCONMSG_TCP_REVERSE:
    return sercon_handle_tcp( cnx, msg );
    break;
  case SERCONMSG_TCP_X11_FORWARD:
    return sercon_handle_x11_forward( cnx, msg );
    break;
  case SERCONMSG_TCP_DATA:
    return sercon_require_req( cnx, msg )
      || sercon_handle_tcp_data( cnx, msg );
    break;
  case SERCONMSG_RAW:
    return sercon_handle_raw( cnx, msg );
    break;
  case SERCONMSG_TTY_SIZE:
    return sercon_handle_tty_sze( cnx, msg);
    break;
  case SERCONMSG_BYE:
    return sercon_handle_bye( cnx, msg );
    break;
  }
}


/* 
 *  sercon_error --
 *  
 *    Depending on the error the function might send en ACK
 *    message to the remote part.
 *    If the message is `unexpected' then no ACK message is sent.
 *
 *    If an error handler has been registered for the connection
 *    it gets called afterwards. It allows to report the error
 *    on the local system.
 *
 *
 *  Side effects:
 *
 *    if the error was a CRC checksum verification failure
 *    the remote part is asked to do the transmission again.
 */
int sercon_error( cnx, status, msg )
     struct sercon_cnx_t *cnx;
     int status;                 /* error code associated to message */
     struct sercon_msg_t *msg;   /* message with error */
{
  switch( status ) {
  case SERCON_ERR_CRC:
    sercon_send_ack( cnx, SERCON_ERR_RETRY, msg );
    break;
  case SERCON_ERR_UNEXPECTED_MSG:
    break;
  }
}

/* 
 *  sercon_send_msg --
 *
 *     This function sends a message on the serial line.
 *
 *     Before the message is sent, the following happens :
 *     The message recieves its serial number which
 *     is computed. The CRC code of the message is computed.
 *     A copy of the message is placed in a queue where
 *     message awaiting an answer are stored.
 *
 *     This function CANNOT be used to send an ACK message
 *     whose serial id is the serial of the message to acknoledge.
 *
 *  Results:
 *     SERCON_OK or an error code.
 *
 *  Side effects
 */
void sercon_send_msg( cnx, msg )
     struct sercon_cnx_t *cnx;
     struct sercon_msg_t *msg;
{
  struct sercon_awm_t *awm;

  /* set message serial number */
  msg->serial = cnx->nextserial;
  cnx->nextserial++;

  /* compute CRC of message */
  msg->crc = sercon_crc_compute( msg );
  
  /* allocate an awm object for message
   * the message is copied inside */
  awm = sercom_alloc_awm( msg );

  /* remember awm */
  sercon_append_awm( awm );

  /* send the message */
  sercon_send_awm( cnx, awm );
}

/* 
 *  sercon_send_ack --
 *
 *     This function sends an ACK message on the serial line.
 *
 *     Before the message is sent, the following happens :
 *     The message recieves its serial number which
 *     is computed. The CRC code of the message is computed.
 *     A copy of the message is placed in a queue where
 *     message awaiting an answer are stored.
 *
 *     This function is for ACK messages ONLY !
 *
 *  Results:
 *     SERCON_OK or an error code.
 *
 *  Side effects
 */
int sercon_send_ack( cnx, status, req, msg )
     struct sercon_cnx_t *cnx;  /* serila connection */
     int status;                /* status to answer */
     struct sercon_req_t *req;  /* if not NULL the associated request */
     struct sercon_msg_t *msg;  /* the associated message */
{
}

/*
 *  sercon_handle_ack --
 *
 *     This function process acknoledgment messages.
 *     It checks if the message is expected, returning an error if not.
 *     If the message status is SERCON_ERR_RETRY, the message is sent again.
 *     If the message is associated to a 'request', the request handler gets called. 
 *     
 */
int sercon_handle_ack( cnx, msg )
     struct sercon_cnx_t *cnx;    /* connection to use */
     struct sercon_msg_t *msg;    /* incoming message to dispatch */
{
  struct sercon_req_t *req = NULL;
  struct sercon_awm_t *awm = NULL;

  for( awm = sercon_first_awm(cnx); awm; awm = sercon_next_awm(cnx,awm) ) {
    if ( awm->msg->serial == msg->serial ) {
      break;
    }
  }
  if ( awm == NULL ) {
    return sercon_error( cnx, SERCON_ERR_UNEXPECTED_MSG, msg);
  }
  if ( msg->reqid != 0 ) {
    for ( req = sercon_first_req(cnx); req; req = sercon_next_req(cnx, req)) {
      if ( req->reqid == msg->reqid ) {
	break;
      }
    }
    if ( req == NULL ) {
      return sercon_error( cnx, SERCON_ERR_UNEXPECTED_MSG, msg);
    }
  }
  
  /* update stats */
  req->recbytes += msg->len;

  /* process message */
  switch ( msg->status ) {
  case SERCON_OK:
    sercon_remove_awm(awm);
    if ( req != NULL ) {
      return sercon_req_handle( cnx, req, msg);
    }
    break;
  case SERCON_ERR_RETRY:
    return sercon_resend_awm( cnx, awm );
    break;
  default:
    /* an error code has been received */
    if ( req == NULL ) {
      return sercon_error( cnx, msg->status, msg );
    }
    else {
      return sercon_req_error( cnx, req, msg->status, msg );
    }
  }
}

/*
 *
 *
 */
int sercon_req_handle( cnx, req, msg )
     struct sercon_cnx_t *cnx;    /* connection to use */
     struct sercon_req_t *req;    /* associated request */
     struct sercon_msg_t *msg;    /* incoming message to dispatch */
{
  return req->handle( cnx, req, msg );
}

int sercon_req_error( cnx, req, status, msg )
     struct sercon_cnx_t *cnx;    /* connection to use */
     struct sercon_req_t *req;    /* associated request */
     int status;                  /* error code */
     struct sercon_msg_t *msg;    /* incoming message to dispatch */
{
  return req->error( cnx, req, status, msg );
}

int sercon_handle_raw( cnx, msg )
{
  // forward message to raw handler
}

/*
 * sercon_handle_send_file --
 *
 *   Remote end wants to send a file.
 *   The message contains the path the remote part
 *   wants to write to. The path will be checked :
 *   if the parent directory doesn't exist or can't be
 *   written to it is an error.
 *
 * Result:
 *
 *   SERCON_OK if everything is fine.
 *   an error code otherwise.
 *
 * Side effects:
 *
 *   A file is created and opened in binary mode.
 *   A request is created and added to the list of request
 *   managed by the connexion.
 *   A reply message is written to remote.
 *
 */
int sercon_handle_send_file( cnx, msg )
     struct sercon_cnx_t *cnx;    /* connection to use */
     struct sercon_msg_t *msg;    /* incoming message to dispatch */
{
  struct sercon_msg_t *ack = NULL;
  struct sercon_req_file_t *req = NULL;
  int status = SERCON_OK;
  FILE *fin;

  /* try to open file for reading
   * give up on failure. */
  fin = fopen( msg->path, "wb" );
  if ( fin == NULL ) {
    status = SERCON_ERR_FROM_ERRNO(errno);
  }
  else {
    req = sercon_alloc_req(sizeof(*req));
    req->flags = SERCON_REQ_FILE_SEND_FLAG;
    req->head.info  = strdup( msg->path );
    req->head.state = SERCONMSG_SEND_FILE;
    sercon_add_req( cnx, req );
  }
  
  ack = sercon_alloc_ack( cnx, status, msg );
  sercon_send_msg( cnx, ack );
  sercon_free_ack( cnx, ack );
  
  return status;
}

/*
 * sercon_handle_send_file_data --
 *
 *   Remote end is sending a piece of file data
 *   If the len field of msg.head is 0 it means
 *   that the EOF has been reached.
 *
 * Results:
 *
 * Side effects:
 *   ACK message is sent back
 */
int sercon_handle_send_file_data( cnx, msg ) 
     struct sercon_cnx_t *cnx;    /* connection to use */
     struct sercon_msg_t *msg;    /* incoming message to dispatch */
{
  struct sercon_msg_t *ack = NULL;
  struct sercon_req_file_t *req = NULL;
  int status = SERCON_OK;

  req = sercon_find_req( cnx, msg );
  assert( req != NULL );

  if ( msg->head.len > 0 ) {
    fwrite( msg->data, len, 1, req->fp );
  }
  else {
    /* close file and delete associated request */
    fclose( req->fp );
    sercon_delete_req( cnx, req );
  }
  
  ack = sercon_alloc_ack( cnx, status, msg );
  sercon_send_msg( cnx, ack );
  sercon_free_ack( cnx, ack );

  return status;
}

/*
 * sercon_handle_recv_file --
 *
 *   Remote end wants to receive a file.
 *   The message contains the path the remote part
 *   wants to get. The path will be checked :
 *   if the file can't be read an error is returned.
 *
 * Result:
 *
 *   SERCON_OK if everything is fine.
 *   an error code otherwise.
 *
 * Side effects:
 *
 *   A file is created and opened in binary mode.
 *   A request is created and added to the list of request
 *   managed by the connexion.
 *   A reply ACK message is written to remote.
 *
 */
int sercon_handle_recv_file( cnx, msg )
     struct sercon_cnx_t *cnx;    /* connection to use */
     struct sercon_msg_t *msg;    /* incoming message to dispatch */
{
  struct sercon_msg_t *ack = NULL;
  struct sercon_req_file_t *req = NULL;
  int status = SERCON_OK;
  FILE *fin;

  /* try to open file for reading
   * give up on failure. */
  fin = fopen( msg->path, "rb" );
  if ( fin == NULL ) {
    status = SERCON_ERR_FROM_ERRNO(errno);
  }
  else {
    req = sercon_alloc_req(sizeof(*req));
    req->flags = SERCON_REQ_FILE_SEND_FLAG;
    req->head.info  = strdup( msg->path );
    req->head.state = SERCONMSG_RECV_FILE;
    sercon_add_req( cnx, req );
  }
  
  ack = sercon_alloc_ack( cnx, status, msg );
  sercon_send_ack( cnx, ack );
  sercon_free_ack( cnx, ack );
  
  return status;
}

/*
 * sercon_handle_tty_size --
 *
 *    This function try to change the size of bound pty.
 *    Typically it will be sent by the remote system when its pty
 *    window change of size.
 *
 * Results:
 *
 *    Returns SERCON_OK or an error code which might derive from POSIX
 *    error code.
 *
 * Side effects:
 *
 *    An ACK message is sent back to the remote system.
 */
int sercon_handle_tty_size( cnx, msg )
     struct sercon_cnx_t *cnx;    /* connection to use */
     struct sercon_msg_t *msg;    /* incoming message to dispatch */
{
  int status = SERCON_OK;
  if ( cnx->fd == -1 ) {
    status = SERCON_ERR_NO_TTY;
  }
  else {
    struct winsize ws = {.ws_row = msg->height, .ws_col = msg->width};
    int rc = ioctl( cnx->fd, TIOCSWINSZ, &ws);
    if ( rc < 0 ) {
      status = SERCON_ERR_FROM_ERRNO(errno);
    }
  }
  
  ack = sercon_alloc_ack( cnx, status, msg );
  sercon_send_ack( cnx, ack );
  sercon_free_ack( cnx, ack );
  
  return status;
}

int sercon_handle_tcp_data( cnx, req, msg )
     struct sercon_cnx_t *cnx;    /* connection to use */
     struct sercon_req_t *req;    /* request */
     struct sercon_msg_t *msg;    /* incoming message to dispatch */
{
  int status = SERCON_OK;
  
  if ( req->state != SERCON_STATE_TCP_DATA ) {
    status = SERCON_ERR_BAD_MESSAGE;
  }
  else {
    switch( req-> ) {
    }
  }
}


struct serconmsg_ack_t sercon_alloc_ack( cnx, status, msg )
     struct sercon_cnx_t *cnx;    /* connection to use */
     int status;
     struct sercon_msg_t *msg;    /* incoming message to dispatch */
{
  struct sercon_msg_t *ack = NULL;
  ack = sercon_alloc_msg(sizeof(struct serconmsg_ack_t));
  if ( ack == NULL ) {
    sercon_panic("memory allocation error");
  }
  ack->id = SERCONMSG_ACK;
  ack->serial = msg->serial;
  ack->status = SERCON_OK;
  ack->reqid = cnx->nextreqid;
  cnx->nextreqid++;
  return ack;
}

/*
 * sercon_handle_bye --
 *
 *   Remote part has closed its connection either by itself
 *   or because we sent a BYE message before.
 *
 * Results:
 *   Always SERCON_OK
 *
 * Side effects:
 *   All opened files and sockets are closed.
 *   The list of ongoing requests is cleared.
 *   The list of messages waiting for an answer is closed.
 */
int sercon_handle_bye( cnx, msg )
{
  /* close all opened files / sockets */

  /* clear the awm and req lists */

  /* send a bye message too (if not done before)
   * maybe the other side will listen to it before closing its serial link */

  return SERCON_OK;
}
