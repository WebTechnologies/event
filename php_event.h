/*
* Zarium libevent обертка
*/


#ifndef PHP_EVENT_H
#define PHP_EVENT_H

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

extern zend_module_entry event_module_entry;
#define phpext_event_ptr &event_module_entry

#ifdef PHP_WIN32
#define PHP_EVENT_API __declspec(dllexport)
#else
#define PHP_EVENT_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

/* refcount макросы */
#ifndef Z_ADDREF_P
#define Z_ADDREF_P(pz)                (pz)->refcount++
#endif

#ifndef Z_DELREF_P
#define Z_DELREF_P(pz)                (pz)->refcount--
#endif

#ifndef Z_SET_REFCOUNT_P
#define Z_SET_REFCOUNT_P(pz, rc)      (pz)->refcount = rc
#endif


PHP_MINIT_FUNCTION(event);
PHP_MSHUTDOWN_FUNCTION(event);
PHP_RINIT_FUNCTION(event);
PHP_RSHUTDOWN_FUNCTION(event);
PHP_MINFO_FUNCTION(event);


PHP_FUNCTION(console_write);

/*
 * Стандартные libevent функции
 */
PHP_FUNCTION(event_init);
PHP_FUNCTION(event_reinit);
PHP_FUNCTION(event_new);
PHP_FUNCTION(event_free);
PHP_FUNCTION(event_set);
PHP_FUNCTION(event_add);
PHP_FUNCTION(event_del);
PHP_FUNCTION(event_dispatch);
PHP_FUNCTION(event_loopbreak);

/*
 * Функции HTTP Client/Server
 */
PHP_FUNCTION(evhttp_start);
PHP_FUNCTION(evhttp_set_gencb);
PHP_FUNCTION(evhttp_connection_new);
PHP_FUNCTION(evhttp_connection_set_closecb);
PHP_FUNCTION(evhttp_request_new);
PHP_FUNCTION(evhttp_request_free);
PHP_FUNCTION(evhttp_make_request);

/*
 * Функции запроса HTTP
 */
PHP_FUNCTION(evhttp_request_get_uri);
PHP_FUNCTION(evhttp_request_method);
PHP_FUNCTION(evhttp_request_body);
PHP_FUNCTION(evhttp_request_input_buffer);
PHP_FUNCTION(evhttp_request_headers);
PHP_FUNCTION(evhttp_request_find_header);
PHP_FUNCTION(evhttp_request_add_header);
PHP_FUNCTION(evhttp_request_status);
PHP_FUNCTION(evhttp_request_append_body);

/*
 * Функции буферов
 */
PHP_FUNCTION(evbuffer_new);
PHP_FUNCTION(evbuffer_free);
PHP_FUNCTION(evbuffer_add);
PHP_FUNCTION(evbuffer_readline);

/*
 * HTTP Функции ответа
 */
PHP_FUNCTION(evhttp_response_set);
PHP_FUNCTION(evhttp_response_add_header);

/*
 * События для bufferevent
 */
PHP_FUNCTION(bufferevent_new);
PHP_FUNCTION(bufferevent_enable);
PHP_FUNCTION(bufferevent_disable);
PHP_FUNCTION(bufferevent_read);
PHP_FUNCTION(bufferevent_write);

PHP_FUNCTION(ntohs);
PHP_FUNCTION(ntohl);
PHP_FUNCTION(htons);
PHP_FUNCTION(htonl);

#define PHP_EVENT_RES_NAME "EVENT"
#define PHP_BUFFEREVENT_RES_NAME "BUFFEREVENT"
#define PHP_EVHTTP_RES_NAME "EVHTTP"
#define PHP_EVHTTP_REQUEST_RES_NAME "EVHTTP Request"
#define PHP_EVHTTP_CONNECTION_RES_NAME "EVHTTP Connection"
#define PHP_EVBUFFER_RES_NAME "EVBUFFER"
#define PHP_EVHTTP_RESPONSE_RES_NAME "EVHTTP Response"

#ifdef ZTS
#define EVENT_G(v) TSRMG(event_globals_id, zend_event_globals *, v)
#else
#define EVENT_G(v) (event_globals.v)
#endif


/*
 * Структура для PHP Event
 */
typedef struct _php_event
{
	zval *stream;
	zval *cb;
	zval *flags;
	zval *event;
#ifdef ZTS
	TSRMLS_D;
#endif
} php_event;

/*
 * Структура bufferevent для PHP Event
 */
typedef struct _php_bufferevent
{
	zval *res_bufferevent;
	zval *stream;
	zval *r_cb;
	zval *w_cb;
	zval *e_cb;
#ifdef ZTS
	TSRMLS_D;
#endif
} php_bufferevent;

/*
 * Структура httpevent для PHP Event
 */
typedef struct _php_httpevent
{
	zval *res_httpevent;
	zval *r_cb;
#ifdef ZTS
	TSRMLS_D;
#endif
} php_httpevent;

/*
 * Стуркутура для PHP Event  (evhttp_connection)
 * необходимую для закрытия соединения HTTP по onClose
 */
typedef struct _php_httpcon
{
	zval *res_httpcon;
	zval *c_cb;
#ifdef ZTS
	TSRMLS_D;
#endif
} php_httpcon;

 
/*
 * Кастомные структуры для HTTP кода ответа и сообщения ассоциированноых
 * с определенным буфером. 
 * Libevent использует evhttp_request структуру для запросов и ответов
*/
typedef struct _evhttp_response
{
	int res_code;
	char* res_message;
	char* res_body;
	int res_body_len;
#ifdef ZTS
	TSRMLS_D;
#endif
} evhttp_response;

typedef struct _evhttp_callback_arg
{
	zval *arg;
#ifdef ZTS
	TSRMLS_D;
#endif
} evhttp_callback_arg;

#endif

