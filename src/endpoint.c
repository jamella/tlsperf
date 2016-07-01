
#include <re.h>
#include "tlsperf.h"


struct tls_endpoint {
	struct tls *tls;
	struct tcp_sock *ts;
	struct tcp_conn *tc;
	struct tls_conn *sc;
	struct sa addr;
	bool client;
	bool established;
	tls_endpoint_estab_h *estabh;
	tls_endpoint_error_h *errorh;
	void *arg;
};


static void destructor(void *arg)
{
	struct tls_endpoint *ep = arg;

	mem_deref(ep->sc);
	mem_deref(ep->tc);
	mem_deref(ep->ts);
	mem_deref(ep->tls);
}


static void conn_close(struct tls_endpoint *ep, int err)
{
	ep->sc = mem_deref(ep->sc);
	ep->tc = mem_deref(ep->tc);

	ep->errorh(err, ep->arg);
}


static void tcp_estab_handler(void *arg)
{
	struct tls_endpoint *ep = arg;

#if 0
	re_printf("[ %s ] established, cipher is %s\n",
		  ep->client ? "Client" : "Server",
		  tls_cipher_name(ep->sc));
#endif

	ep->established = true;

	ep->estabh(tls_cipher_name(ep->sc), ep->arg);
}


static void tcp_close_handler(int err, void *arg)
{
	struct tls_endpoint *ep = arg;

	conn_close(ep, err);
}


static void tcp_conn_handler(const struct sa *peer, void *arg)
{
	struct tls_endpoint *ep = arg;
	int err;

	err = tcp_accept(&ep->tc, ep->ts, tcp_estab_handler,
			 0, tcp_close_handler, ep);

	if (err) {
		conn_close(ep, err);
		return;
	}

	err = tls_start_tcp(&ep->sc, ep->tls, ep->tc, 0);
	if (err) {
		conn_close(ep, err);
		return;
	}
}


int tls_endpoint_alloc(struct tls_endpoint **epp, struct tls *tls,
		       bool client,
		       tls_endpoint_estab_h *estabh,
		       tls_endpoint_error_h *errorh, void *arg)
{
	struct tls_endpoint *ep;
	int err = 0;

	ep = mem_zalloc(sizeof(*ep), destructor);

	ep->tls = mem_ref(tls);
	ep->client = client;
	ep->estabh = estabh;
	ep->errorh = errorh;
	ep->arg = arg;

	sa_set_str(&ep->addr, "127.0.0.1", 0);

	if (client) {

	}
	else {
		err = tcp_listen(&ep->ts, &ep->addr, tcp_conn_handler, ep);
		if (err)
			goto out;

		err = tcp_sock_local_get(ep->ts, &ep->addr);
		if (err)
			goto out;
	}

 out:
	if (err)
		mem_deref(ep);
	else if (epp)
		*epp = ep;

	return err;
}


int tls_endpoint_start(struct tls_endpoint *ep, const struct sa *addr)
{
	int err;

	if (!ep)
		return EINVAL;

	if (ep->client) {

		err = tcp_connect(&ep->tc, addr,
				  tcp_estab_handler, NULL,
				  tcp_close_handler, ep);
		if (err)
			return err;

		err = tls_start_tcp(&ep->sc, ep->tls, ep->tc, 0);
		if (err)
			return err;
	}
	else {

	}

	return 0;
}


const struct sa *tls_endpoint_addr(const struct tls_endpoint *ep)
{
	return ep ? &ep->addr : NULL;
}


bool tls_endpoint_established(const struct tls_endpoint *ep)
{
	return ep ? ep->established : false;
}