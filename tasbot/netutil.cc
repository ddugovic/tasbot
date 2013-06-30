
#include <string>

#include "netutil.h"
#include "SDL.h"
#include "SDL_net.h"

using namespace std;

string IPString(const IPaddress &ip) {
  // XXX assumes little-endian
  int port = ((ip.port & 255) << 8) | (255 & (ip.port >> 8));
  return StringPrintf("%d.%d.%d.%d:%d",
                      255 & ip.host,
                      255 & (ip.host >> 8),
                      255 & (ip.host >> 16),
                      255 & (ip.host >> 24),
                      port);
}

TCPsocket ConnectLocal(int port) {
  IPaddress ip;
  TCPsocket tcpsock;

  if (SDLNet_ResolveHost(&ip, "localhost", port) == -1) {
    fprintf(stderr, "SDLNet_ResolveHost: %s\n", SDLNet_GetError());
    abort();
  }

  tcpsock = SDLNet_TCP_Open(&ip);
  if (!tcpsock) {
    fprintf(stderr, "SDLNet_TCP_Open(%s): %s\n", 
	    IPString(ip).c_str(),
	    SDLNet_GetError());
    abort();
  }

  return tcpsock;
}

void BlockOnSocket(TCPsocket sock) {
  SDLNet_SocketSet sockset = SDLNet_AllocSocketSet(1);
  if (!sockset) {
    fprintf(stderr, "SDLNet_AllocSocketSet: %s\n", SDLNet_GetError());
    abort();
  }

  CHECK(-1 != SDLNet_TCP_AddSocket(sockset, sock));

  for (;;) {
    int numready = SDLNet_CheckSockets(sockset, -1);
    if (numready == -1) {
      fprintf(stderr, "SDLNet_CheckSockets: %s\n", SDLNet_GetError());
      perror("SDLNet_CheckSockets");
      abort();
    }

    if (numready > 0) {
      break;
    }
  }

  // Just one socket in the set, so it should be ready.
  CHECK(SDLNet_SocketReady(sock));

  SDLNet_FreeSocketSet(sockset);
}

SingleServer::SingleServer(int port) : port_(port), state_(LISTENING) {
  peer_ = NULL;
  if (SDLNet_ResolveHost(&localhost_, NULL, port_) == -1) {
    fprintf(stderr, "SDLNet_ResolveHost: %s\n", SDLNet_GetError());
    abort();
  }

  server_ = SDLNet_TCP_Open(&localhost_);
  if (!server_) {
    fprintf(stderr, "SDLNet_TCP_Open: %s\n", SDLNet_GetError());
    abort();
  }
}

void SingleServer::Listen() {
  CHECK(state_ == LISTENING);

  for (;;) {
    BlockOnSocket(server_);
    if ((peer_ = SDLNet_TCP_Accept(server_))) {
      IPaddress *peer_ip_ptr = SDLNet_TCP_GetPeerAddress(peer_);
      if (peer_ip_ptr == NULL) {
        printf("SDLNet_TCP_GetPeerAddress: %s\n", SDLNet_GetError());
        abort();
      }

      peer_ip_ = *peer_ip_ptr;

      state_ = ACTIVE;
      return;
    }

    fprintf(stderr, "Socket was ready but couldn't accept?\n");
    SDL_Delay(1000);
  }
}

string SingleServer::PeerString() {
  CHECK(state_ == ACTIVE);
  return IPString(peer_ip_);
}

void SingleServer::Hangup() {
  if (state_ == ACTIVE) {
    SDLNet_TCP_Close(peer_);
    peer_ = NULL;
  }

  state_ = LISTENING;
}

bool RecvErrorRetry() {
  // XXX consult SDLNet_GetLastError()
  return false;
}

RequestCache::RequestCache(int size) : size(size), num(0) {}

extern int sdlnet_recvall(TCPsocket sock, void *buffer, int len) {
  int alreadyread = 0;
  while (len > 0) {
    int ret = SDLNet_TCP_Recv(sock, (void *)buffer, len);
    if (ret <= 0) return ret;
    // ?
    else if (ret > len) return -1;
    else if (ret == len) return alreadyread + ret;
    else if (errno != 0) return alreadyread + ret;
    else {
      // fprintf(stderr, "Partial read of %d; %d left.\n",
      // ret, len - ret);
      // Probably not an error, but a partial read.
      alreadyread += ret;
      len -= ret;
      CHECK(len >= 0);
      // Buffer now points to the remaining area.
      buffer = (void*) (((char*)buffer) + ret);
    }
  }
  return alreadyread;
}
