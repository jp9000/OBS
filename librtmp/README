RTMP Dump v2.4
(C) 2009 Andrej Stepanchuk
(C) 2009-2011 Howard Chu
(C) 2010 2a665470ced7adb7156fcef47f8199a6371c117b8a79e399a2771e0b36384090
(C) 2011 33ae1ce77301f4b4494faaa5f609f3c48b9dcf82
License: GPLv2
librtmp license: LGPLv2.1
http://rtmpdump.mplayerhq.hu/

To compile type "make" with SYS=<platform name>, e.g.

  $ make SYS=posix

for Linux, Unix, etc. or

  $ make SYS=darwin

for MacOSX or

  $ make SYS=mingw

for Windows.

You can cross-compile for other platforms using the CROSS_COMPILE variable:

  $ make CROSS_COMPILE=arm-none-linux- INC=-I/my/cross/includes

Please read the Makefile to see what other make variables are used.

This code also requires you to have OpenSSL and zlib installed. You may
optionally use GnuTLS or polarssl instead of OpenSSL if desired. You may
also build with just rtmpe support, and no rtmps/https support, by
specifying -DNO_SSL in the XDEF macro, e.g.

  $ make XDEF=-DNO_SSL

or

  $ make CRYPTO=POLARSSL XDEF=-DNO_SSL

You may also turn off all crypto support if desired

  $ make CRYPTO=

A shared library is now built by default, in addition to the static
library. You can also turn it off if desired

  $ make SHARED=

The rtmpdump programs still link to the static library, regardless.

Note that if using OpenSSL, you must have version 0.9.8 or newer.
For Polar SSL you must have version 1.0.0 or newer.

Credit goes to team boxee for the XBMC RTMP code originally used in RTMPDumper.
The current code is based on the XBMC code but rewritten in C by Howard Chu.


SWF Verification
----------------

Note: these instructions for manually generating the SWFVerification
info are provided only for historical documentation. The software can now
generate this info automatically, so it is no longer necessary to
run the commands described here. Just use the -W (--swfVfy) option
to perform automatic SWFVerification.

Download the swf player you want to use for SWFVerification, unzip it using

 $ flasm -x file.swf

It will show the decompressed filesize, use it for --swfsize

Now generate the hash

 $ openssl sha -sha256 -hmac "Genuine Adobe Flash Player 001" file.swf

and use the --swfhash "01234..." option to pass it.

e.g. $ ./rtmpdump --swfhash "123456..." --swfsize 987...


Connect Parameters
------------------

Some servers expect additional custom parameters to be attached to the
RTMP connect request. The "--auth" option handles a specific case, where
a boolean TRUE followed by the given string are added to the request.
Other servers may require completely different parameters, so the new
"--conn" option has been added. This option can be set multiple times
on the command line, adding one parameter each time.

The argument to the option must take the form <type> : <value> where
type can be B for boolean, S for string, N for number, and O for object.
For booleans the value must be 0 or 1. Also, for objects the value must
be 1 to start a new object, or 0 to end the current object.

Examples:
  --conn B:0 --conn S:hello --conn N:3.14159

Named parameters can be specified by prefixing 'N' to the type. Then the
name should come next, and finally the value:
  --conn NB:myflag:1 --conn NS:category:something --conn NN:pi:3.14159

Objects may be added sequentially:
  -C O:1 -C NB:flag:1 -C NS:status:success -C O:0 -C O:1 -C NN:time:12.30 -C O:0
or nested:
  -C O:1 -C NS:code:hello -C NO:extra:1 -C NS:data:stuff -C O:0 -C O:0


Building OpenSSL 0.9.8k
-----------------------
arm:
./Configure -DL_ENDIAN --prefix=`pwd`/armlibs linux-generic32

Then replace gcc, cc, ar, ranlib in Makefile and crypto/Makefile by arm-linux-* variants  and use make && make install_sw

win32:
Try ./Configure mingw --prefix=`pwd`/win32libs -DL_ENDIAN -DOPENSSL_NO_HW
Replace gcc, cc, ... by mingw32-* variants in Makefile and crypto/Makefile
make && make install_sw

OpenSSL cross-compiling can be a difficult beast.

Precompiled OpenSSL binaries for Windows are available on
http://www.slproweb.com/products/Win32OpenSSL.html

If you're just running a pre-built Windows rtmpdump binary, then all you
need is the "Light" installer. If you want to compile rtmpdump yourself,
you'll need the full installer.


Example Servers
---------------
Three different types of servers are also present in this distribution:
 rtmpsrv - a stub server
 rtmpsuck - a transparent proxy
 rtmpgw - an RTMP to HTTP gateway

rtmpsrv - Note that this is very incomplete code, and I haven't yet decided
whether or not to finish it. It is useful for obtaining all the parameters
that a real Flash client would send to an RTMP server, so that they can be
used with rtmpdump. The current version now invokes rtmpdump automatically
after parsing a client request.

rtmpsuck - proxy server. See below...

All you need to do is redirect your Flash clients to the machine running this
server and it will dump out all the connect / play parameters that the Flash
client sent. The simplest way to cause the redirect is by editing /etc/hosts
when you know the hostname of the RTMP server, and point it to localhost while
running rtmpsrv on your machine. (This approach should work on any OS; on
Windows you would edit %SystemRoot%\system32\drivers\etc\hosts.)

On Linux you can also use iptables to redirect all outbound RTMP traffic. You
need to be running as root in order to use the iptables command.

In my original plan I would have the transparent proxy running as a special
user (e.g. user "proxy"), and regular Flash clients running as any other user.
In that case the proxy would make the connection to the real RTMP server. The
iptables rule would look like this:

iptables -t nat -A OUTPUT -p tcp --dport 1935 -m owner \! --uid-owner proxy \
 -j REDIRECT

A rule like the above will be needed to use rtmpsuck. Note that you should
replace "proxy" in the above command with an account that actually exists
on your machine.

Using it in this mode takes advantage of the Linux support for IP redirects;
in particular it uses a special getsockopt() call to retrieve the original
destination address of the connection. That way the proxy can create the
real outbound connection without any other help from the user. The equivalent
functionality may exist on other OSs but needs more investigation.

(Based on reading the BSD ipfw manpage, this rule ought to work on BSD:

ipfw add 40 fwd 127.0.0.1,1935 tcp from any to any 1935 not uid proxy

Some confirmation from any BSD users would be nice.)

(We have a solution for Windows based on a TDI driver; this is known to
work on Win2K and WinXP but is assumed to not work on Vista or Win7 as the
TDI is no longer used on those OS versions. Also, none of the known
solutions are available as freeware.)

The rtmpsuck command has only one option: "-z" to turn on debug logging.
It listens on port 1935 for RTMP sessions, but you can also redirect other
ports to it as needed (read the iptables docs). It first performs an RTMP
handshake with the client, then waits for the client to send a connect
request. It parses and prints the connect parameters, then makes an
outbound connection to the real RTMP server. It performs an RTMP handshake
with that server, forwards the connect request, and from that point on it
just relays packets back and forth between the two endpoints.

It also checks for a few packets that it treats specially: a play packet
from the client will get parsed so that the playpath can be displayed. It
also handles SWF Verification requests from the server, without forwarding
them to the client. (There would be no point, since the response is tied to
each session's handshake.)

Once the play command is processed, all subsequent audio/video data received
from the server will be written to a file, as well as being delivered back
to the client.

The point of all this, instead of just using a sniffer, is that since rtmpsuck
has performed real handshakes with both the client and the server, it can
negotiate whatever encryption keys are needed and so record the unencrypted
data.

rtmpgw - HTTP gateway: this is an HTTP server that accepts requests that
consist of rtmpdump parameters. It then connects to the specified RTMP
server and returns the retrieved data in the HTTP response. The only valid
HTTP request is "GET /" but additional options can be provided in normal
URL-encoded fashion. E.g.
  GET /?r=rtmp:%2f%2fserver%2fmyapp&y=somefile HTTP/1.0

is equivalent the rtmpdump parameters "-r rtmp://server/myapp -y somefile".

Note that only the shortform (single letter) rtmpdump options are supported.
