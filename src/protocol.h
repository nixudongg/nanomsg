/*
    Copyright (c) 2012-2013 250bpm s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#ifndef NN_PROTOCOL_INCLUDED
#define NN_PROTOCOL_INCLUDED

#include "utils/aio.h"
#include "utils/cond.h"
#include "utils/list.h"
#include "utils/clock.h"
#include "utils/msg.h"
#include "utils/efd.h"

#include <stddef.h>
#include <stdint.h>

/******************************************************************************/
/*  Pipe class.                                                               */
/******************************************************************************/

/*  Any combination of following flags can be returned from successful call
    to nn_pipe_send or nn_pipe_recv. */

/*  This flag means that the pipe can't be used for receiving (when returned
    from nn_pipe_recv()) or sending (when returned from nn_pipe_send()).
    Protocol implementation should not send/recv messages from the pipe until
    the pipe is revived by in()/out() function. */
#define NN_PIPE_RELEASE 1

/*  Specifies that received message is already split into header and body.
    This flag is used only by inproc transport to avoid merging and re-splitting
    the messages passed with a single process. */
#define NN_PIPE_PARSED 2

struct nn_pipe;

/*  Associates opaque pointer to protocol-specific data with the pipe. */
void nn_pipe_setdata (struct nn_pipe *self, void *data);

/*  Retrieves the opaque pointer associated with the pipe. */
void *nn_pipe_getdata (struct nn_pipe *self);

/*  Send the message to the pipe. If successful, pipe takes ownership of the
    messages. */
int nn_pipe_send (struct nn_pipe *self, struct nn_msg *msg);

/*  Receive a message from a pipe. 'msg' should not be initialised prior to
    the call. It will be initialised when the call succeeds. */
int nn_pipe_recv (struct nn_pipe *self, struct nn_msg *msg);

/******************************************************************************/
/*  Base class for all socket types.                                          */
/******************************************************************************/

struct nn_sockbase;

/*  To be implemented by individual socket types. */
struct nn_sockbase_vfptr {
    void (*destroy) (struct nn_sockbase *self);
    int (*add) (struct nn_sockbase *self, struct nn_pipe *pipe);
    void (*rm) (struct nn_sockbase *self, struct nn_pipe *pipe);
    int (*in) (struct nn_sockbase *self, struct nn_pipe *pipe);
    int (*out) (struct nn_sockbase *self, struct nn_pipe *pipe);
    int (*send) (struct nn_sockbase *self, struct nn_msg *msg);
    int (*recv) (struct nn_sockbase *self, struct nn_msg *msg);
    int (*setopt) (struct nn_sockbase *self, int level, int option,
        const void *optval, size_t optvallen);
    int (*getopt) (struct nn_sockbase *self, int level, int option,
        void *optval, size_t *optvallen);
    int (*sethdr) (struct nn_msg *msg, const void *hdr, size_t hdrlen);
    int (*gethdr) (struct nn_msg *msg, void *hdr, size_t *hdrlen);
};

/*  The members of this structure are used exclusively by the core. Never use
    or modify them directly from the protocol implementation. */
struct nn_sockbase
{
    const struct nn_sockbase_vfptr *vfptr;
    int flags;
    struct nn_cp cp;
    struct nn_cond cond;
    struct nn_efd sndfd;
    struct nn_efd rcvfd;
    struct nn_efd errfd;
    struct nn_clock clock;
    int fd;
    struct nn_list eps;
    int eid;
    int linger;
    int sndbuf;
    int rcvbuf;
    int sndtimeo;
    int rcvtimeo;
    int reconnect_ivl;
    int reconnect_ivl_max;
    int sndprio;
    int rcvprio;
};

/*  Initialise the socket. */
void nn_sockbase_init (struct nn_sockbase *self,
    const struct nn_sockbase_vfptr *vfptr, int fd);

/*  Uninitialise the socket. */
void nn_sockbase_term (struct nn_sockbase *self);

/*  If recv is blocking at the moment, this function will unblock it. */
void nn_sockbase_unblock_recv (struct nn_sockbase *self);

/*  If send is blocking at the moment, this function will unblock it. */
void nn_sockbase_unblock_send (struct nn_sockbase *self);

/*  Returns the completion port associated with the socket. */
struct nn_cp *nn_sockbase_getcp (struct nn_sockbase *self);

/******************************************************************************/
/*  The socktype class.                                                       */
/******************************************************************************/

/*  This structure defines a class factory for individual socket types. */

struct nn_socktype {

    /*  Domain and protocol IDs as specified in nn_socket() function. */
    int domain;
    int protocol;

    /*  Function to create the socket type. This function is called under
        global lock, so it is not possible that two sockets are being
        created in parallel. */
    struct nn_sockbase *(*create) (int fd);

    /*  This member is owned by the core. Never touch it directly from inside
        the protocol implementation. */
    struct nn_list_item list;
};

#endif
