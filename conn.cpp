#include "conn.hpp"
#include <cctype>


std::vector<char> SetNumber(int vv) {
    std::vector<char> v(128, 0);
    int d = vv;
    if (vv == 1) {
        sprintf(&v[0], "\n    { 0, 1 }");
        v[13] = '\0';
        return v;
    }
    sprintf(&v[0], "\n     { ");
    int j = 8;
    int x;
    int k = 0;
    while (vv > 1) {
        for (int i = 2; i <= d; ++i)
            if (vv % i == 0) {
                ++k;
                vv /= i;
                sprintf(&v[0] + j, "%d %n", i, &x);
                j += x;
                break;
            }
    }
    sprintf(&v[0] + j, ",  %d,  %d }%n", k, d, &x);
    v[j + x] = '\0';
    return v;
}

int skip(const char *message) {
    int i;
    for (i = 0; isblank(message[i]) || isalpha(message[i]); ++i) {
    }
    return i;
}

void SetVector(const char *message, std::vector<char> &obuf) {
    std::vector<int> vv;
    vv.clear();
    char *pointer = (char *)message;
    char *end = pointer + strlen(message) - 1;
    while (pointer < end) {
        pointer = &pointer[skip(pointer)];
        int d;
        int x;
        sscanf(pointer, "%d%n", &d, &x);
        pointer = &pointer[x];
        vv.push_back(d);
    }
    for (size_t i = 0; i < vv.size(); ++i) {
        std::vector<char> number;
        number = SetNumber(vv[i]);
        obuf.insert(obuf.end(), &number[0], &number[0] + strlen(&number[0]));
    }
    obuf.push_back('\0');
}

Conn::Conn(int fd): fd_(fd), actv_(true), icur_(0), iall_(4), ocur_(0), oall_(0), ibuf_(4, 0), obuf_(4, 0) {}

Conn::~Conn() {
    close_();
}

bool Conn::canRcv() const {
    return actv_ && iall_ > icur_;
}

bool Conn::canSnd() const {
    return actv_ && oall_ > ocur_;
}

bool Conn::isActv() const {
    return actv_;
}

static const char *prefix = "Server:\n";

void Conn::rcv() {
    uint32_t h;
    ssize_t  s;

    if(!canRcv()) return;

    s = read(fd_, &ibuf_[icur_], iall_ - icur_);

    if(s > 0)
        icur_ += s;
    else
        if(!s) {
            close_();
            return;
        }
        else
            if(s == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                fprintf(stderr, "%4d - Read error.\n", fd_);
                close_();
                return;
            }

    if(iall_ == icur_) {
        if(iall_ == sizeof h) {
            memcpy(&h, &ibuf_[0], sizeof h);

            ibuf_.resize(h + sizeof h, 0);
            iall_ += h;
        }
        else {
            std::vector<char> v;
            printf("\nInput numbers from %d: %s\n", fd_, &ibuf_[4]);
            SetVector(&ibuf_[0] + sizeof(h), v);

            h     = strlen(prefix) + v.size();
            oall_ = h + sizeof h;

            obuf_.resize(oall_, 0);
            memcpy(&obuf_[0], &h, sizeof h);
            sprintf(&obuf_[0] + sizeof h, "%s%s", prefix, &v[0]);
        }
    }
}

void Conn::snd() {
    if(!canSnd()) return;

    ssize_t s = write(fd_, &obuf_[0] + ocur_, oall_ - ocur_);

    if(s > 0)
        ocur_ += s;
    else
        if(!s) {
            close_();
            return;
        }
        else
            if(s == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                fprintf(stderr, "%4d - Write error.\n", fd_);
                close_();
                return;
            }

    if(oall_ == ocur_) {
        icur_ = 0;
        iall_ = 4;
        ocur_ = 0;
        oall_ = 0;
    }
}

void Conn::close_() {
    if(actv_) {
        printf("%4d - Close.\n", fd_);

        if(shutdown(fd_, 2) == -1)
            fprintf(stderr, "%4d - Shutdown error.\n", fd_);

        if(close(fd_))
            fprintf(stderr, "%4d - Close error.\n", fd_);

        actv_ = false;
    }
}


ConnDb::ConnDb(int ld, size_t nc): ld_(ld), nc_(nc), cs_(nc, 0), ps_(nc + 1) {
    ps_[0].fd = ld;
    ps_[0].events = POLLIN;
}

ConnDb::~ConnDb() {
    for(size_t d = 0; d < nc_; d ++)
        delete cs_[d];
}

void ConnDb::perform() {
    nfds_t np;    
    size_t d;

    for(np = 1, d = 0; d < nc_; d ++)
        if(cs_[d] && cs_[d]->isActv()) {
            short e = 0;

            if(cs_[d]->canRcv())
                e = POLLIN;


            if(cs_[d]->canSnd())
                e = e ? e | POLLOUT : POLLOUT;

            if(e) { 
                ps_[np].fd     = d;
                ps_[np].events = e;
                np ++;
            }
        }

    switch(poll(&ps_[0], np, 1 * 1000)) {
        case -1:
            if(errno != EINTR)
                fprintf(stderr, "     - Poll error.\n"); 
            break;

        default:
            if(ps_[0].revents & POLLIN) 
                accept_();

            for(d = 1; d < np; d ++) {
                if(ps_[d].revents & POLLIN)
		    cs_[ps_[d].fd]->rcv();

                if(ps_[d].revents & POLLOUT)
                    cs_[ps_[d].fd]->snd();

            }
    }
}

static void printConn(int fd, struct sockaddr_in *addr);

void ConnDb::accept_() {
    struct sockaddr_in addr;
    socklen_t          addrlen;
    int                fd;

    memset(&addr, 0, sizeof addr);
    addrlen = sizeof addr;

    fd = accept(ld_, (struct sockaddr *)&addr, &addrlen);
    if(fd == -1) {
        if(fd == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
            fprintf(stderr, "     - Accept error.\n");
        return;
    }

    printConn(fd, &addr);

    if((size_t)fd >= nc_) {
        fprintf(stderr, "%4d - Storage limit error.\n", fd);
        Conn c(fd);
    }
    else
        if(fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
            fprintf(stderr, "%4d - Nonblock error.\n", fd);
            Conn c(fd);
        }
        else {
            delete cs_[fd];
            cs_[fd] = new Conn(fd);
        }
}

static void printConn(int fd, struct sockaddr_in *addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof ip);
    printf("%4d -New (%s %d).\n", fd, ip, addr->sin_port);
}

int toUInt16(const char *str, uint16_t *p) {
    long  l;
    char *e;

    errno = 0;
    l = strtol(str, &e, 10);

    if(!errno && *e == '\0' && 0 <= l && l <= 65535) {
        *p = (uint16_t)l;
        return 0;
    }
    else
        return -1;
}

