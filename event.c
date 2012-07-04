/*
* Zarium libevent обертка
*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_event.h"

#ifndef HAVE_TAILQFOREACH
#include <sys/queue.h>
#endif

#include <event2/event.h>
#include <event2/http.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <event2/buffer_compat.h>
#include <event2/bufferevent_compat.h>

#include <event2/bufferevent_struct.h>
#include <event2/event-config.h>
#include <event2/event_compat.h>
#include <event2/event_struct.h>
#include <event2/http_compat.h>
#include <event2/http_struct.h>
#include <event2/listener.h>




#include <arpa/inet.h>

#include <zend_exceptions.h>

/*
 * Если определеных глобальные данные в php_event.h, то надо раскаментировать
 ZEND_DECLARE_MODULE_GLOBALS(event)
*/


/* Глобальные ресурсы, они необходимы для thread safe */
static int le_evhttp;
static int le_evhttp_request;
static int le_evbuffer;
static int le_event;
static int le_bufferevent;
static int le_evhttp_connection;
static int le_evhttp_response;

static struct event_base *current_base; 

/* Деструкторы */
static void event_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC);
static void bufferevent_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC);
static void evhttp_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC);
static void evhttp_request_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC);
static void evhttp_connection_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC);
static void evhttp_response_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC);

/* Опредление аргументов функций для декларирования */
#ifdef ZEND_ENGINE_2
    ZEND_BEGIN_ARG_INFO(bufferevent_read_byref_arginfo, 0)
        ZEND_ARG_PASS_INFO(0)
        ZEND_ARG_PASS_INFO(1)
        ZEND_ARG_PASS_INFO(0)
    ZEND_END_ARG_INFO()
#else
static unsigned char bufferevent_read_byref_arginfo[] = { 2, BYREF_FORCE, BYREF_NONE };
#endif


/* 
 *
 * Описание функций, которые используются PHP
 */
zend_function_entry event_functions[] = {
    PHP_FE(console_write, NULL)
    PHP_FE(event_init, NULL)
    PHP_FE(event_reinit, NULL)
    PHP_FE(event_new, NULL)
    PHP_FE(event_free, NULL)
    PHP_FE(event_set, NULL)
    PHP_FE(event_add, NULL)
    PHP_FE(event_del, NULL)
    PHP_FE(event_dispatch, NULL) 
    PHP_FE(event_loopbreak, NULL)
    PHP_FE(evhttp_start, NULL)
    PHP_FE(evhttp_connection_new, NULL)
    PHP_FE(evhttp_connection_set_closecb, NULL)
    PHP_FE(evhttp_request_new, NULL)
    PHP_FE(evhttp_request_free, NULL)
    PHP_FE(evhttp_make_request, NULL)
	PHP_FE(evhttp_set_gencb, NULL) 
    PHP_FE(evhttp_request_get_uri, NULL)
    PHP_FE(evhttp_request_method, NULL)
    PHP_FE(evhttp_request_body, NULL)
    PHP_FE(evhttp_request_append_body, NULL)
	PHP_FE(evhttp_request_input_buffer, NULL)
	PHP_FE(evhttp_request_find_header, NULL)
    PHP_FE(evhttp_request_headers, NULL)
	PHP_FE(evhttp_request_add_header, NULL)
	PHP_FE(evhttp_request_status, NULL)
    PHP_FE(evhttp_response_set, NULL)
	PHP_FE(evhttp_response_add_header, NULL)
	PHP_FE(evbuffer_new, NULL)
    PHP_FE(evbuffer_free, NULL)
    PHP_FE(evbuffer_add, NULL)
	PHP_FE(evbuffer_readline, NULL)
	PHP_FE(bufferevent_new, NULL)
	PHP_FE(bufferevent_enable, NULL)
	PHP_FE(bufferevent_disable, NULL)
	PHP_FE(bufferevent_read, bufferevent_read_byref_arginfo)
	PHP_FE(bufferevent_write, NULL)
	PHP_FE(ntohs, NULL)
	PHP_FE(ntohl, NULL)
	PHP_FE(htons, NULL)
	PHP_FE(htonl, NULL)

	{NULL, NULL, NULL}
};

/* 
 * Вход в модуль event 
 */
zend_module_entry event_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"event",
	event_functions,
	PHP_MINIT(event),
	PHP_MSHUTDOWN(event),
	NULL,
	NULL,
	PHP_MINFO(event),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_EVENT
ZEND_GET_MODULE(event)
#endif

/* Хм.... возможно будут нужны настройки в ini
static void php_event_init_globals(zend_event_globals *event_globals)
{
	event_globals->global_value = 0;
	event_globals->global_string = NULL;
}
*/

/* PHP_MINIT_FUNCTION */

PHP_MINIT_FUNCTION(event)
{
	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/
    
	/* ресурсы */
	le_evhttp = zend_register_list_destructors_ex(evhttp_dtor, NULL, PHP_EVHTTP_RES_NAME, module_number);
    le_evhttp_request = zend_register_list_destructors_ex(evhttp_request_dtor, NULL, PHP_EVHTTP_REQUEST_RES_NAME, module_number);
    le_evhttp_connection = zend_register_list_destructors_ex(evhttp_connection_dtor, NULL, PHP_EVHTTP_CONNECTION_RES_NAME, module_number);
    le_evbuffer = zend_register_list_destructors_ex(NULL, NULL, PHP_EVBUFFER_RES_NAME, module_number);
    le_event = zend_register_list_destructors_ex(event_dtor, NULL, PHP_EVENT_RES_NAME, module_number);
    le_bufferevent = zend_register_list_destructors_ex(bufferevent_dtor, NULL, PHP_BUFFEREVENT_RES_NAME, module_number);
    le_evhttp_response = zend_register_list_destructors_ex(evhttp_response_dtor, NULL, PHP_EVHTTP_RESPONSE_RES_NAME, module_number);

    /* константы */
    REGISTER_LONG_CONSTANT("EV_READ", EV_READ, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("EV_WRITE", EV_WRITE, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("EV_TIMEOUT", EV_TIMEOUT, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("EV_SIGNAL", EV_SIGNAL, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("EV_PERSIST", EV_PERSIST, CONST_CS | CONST_PERSISTENT);
    
    REGISTER_LONG_CONSTANT("EVBUFFER_EOF", EVBUFFER_EOF, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("EVBUFFER_ERROR", EVBUFFER_ERROR, CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("EVHTTP_REQ_GET", EVHTTP_REQ_GET, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("EVHTTP_REQ_POST", EVHTTP_REQ_POST, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("EVHTTP_REQ_HEAD", EVHTTP_REQ_HEAD, CONST_CS | CONST_PERSISTENT);
 
    REGISTER_LONG_CONSTANT("EVHTTP_REQUEST", EVHTTP_REQUEST, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("EVHTTP_RESPONSE", EVHTTP_RESPONSE, CONST_CS | CONST_PERSISTENT);
    
	return SUCCESS;
}

/* PHP_MSHUTDOWN_FUNCTION */

PHP_MSHUTDOWN_FUNCTION(event)
{
	/* Если будем юзать ini
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}

PHP_MINFO_FUNCTION(event)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Zarium libevent support", "enabled");
	php_info_print_table_end();

	/* ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


PHP_FUNCTION(console_write)
{
    php_printf(".\n");
}

PHP_FUNCTION(event_init)
{
    current_base = event_init();
}

PHP_FUNCTION(event_reinit)
{
    event_reinit(current_base);
}


PHP_FUNCTION(event_dispatch)
{
    event_dispatch();
}

PHP_FUNCTION(event_loopbreak)
{
	if (event_loopbreak() == 0)
	{
		RETURN_TRUE;
	}
	else
	{
		RETURN_FALSE;
	}
}

/*
 * Создание структуры события (event) и регистрация его как ресурс
 */
PHP_FUNCTION(event_new)
{
	struct event *e;

    if (!(e = malloc(sizeof(struct event)))) 
    {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "Could not create event resource");
        RETURN_FALSE;
    }
    e->ev_arg = NULL;
    
    //php_printf("Event created...\n");
    ZEND_REGISTER_RESOURCE(return_value, e, le_event);
}

/*
 * Деструктор для  ресурса bufferevent
 */
static void bufferevent_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    struct bufferevent *e = (struct bufferevent*)rsrc->ptr;
    int i;
    
  	if (e->cbarg != NULL)
   	{
   		zval_dtor(((php_bufferevent*)(e->cbarg))->r_cb);
   		FREE_ZVAL(((php_bufferevent*)(e->cbarg))->r_cb);
   		zval_dtor(((php_bufferevent*)(e->cbarg))->w_cb);
   		FREE_ZVAL(((php_bufferevent*)(e->cbarg))->w_cb);
   		zval_dtor(((php_bufferevent*)(e->cbarg))->e_cb);
   		FREE_ZVAL(((php_bufferevent*)(e->cbarg))->e_cb);
   		zval_dtor(((php_bufferevent*)(e->cbarg))->stream);
   		zend_list_delete( Z_RESVAL_P(((php_event*)(e->cbarg))->stream) );
   		FREE_ZVAL(((php_bufferevent*)(e->cbarg))->stream);
   		FREE_ZVAL(((php_bufferevent*)(e->cbarg))->res_bufferevent);
   		free(e->cbarg);
   	}
    bufferevent_free(e);
}


/*
 * Деструктор для evhttp ресурса
 */
static void evhttp_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    evhttp_free((struct evhttp*)rsrc->ptr);
}

/*
 * Деструктор для evhttp_request ресурса
 */
static void evhttp_request_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	struct evhttp_request *r = (struct evhttp_request*)rsrc->ptr;
	if (r->cb_arg != NULL)
   	{
   		zval_dtor(((php_httpevent*)(r->cb_arg))->r_cb);
   		FREE_ZVAL(((php_httpevent*)(r->cb_arg))->r_cb);
   		FREE_ZVAL(((php_httpevent*)(r->cb_arg))->res_httpevent);
   		free(r->cb_arg);
   	}
    //evhttp_request_free(r);
}

/*
 * Деструктор для evhttp_connection ресурса
 */
static void evhttp_connection_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    evhttp_connection_free((struct evhttp_connection*)rsrc->ptr);
}

/*
 * Деструктор для всех evhttp_response ресурсов
 */
static void evhttp_response_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    evhttp_response *r = (evhttp_response*)rsrc->ptr;
    free(r->res_message);
    if (r->res_body_len > 0) 
    {
        free(r->res_body);
    }
    free(r);
}


/*
 * Деструктор для event ресурса (!!! Самаое главное!)
 */
static void event_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    struct event *e = (struct event*)rsrc->ptr;
    int i;
    
    if (event_initialized(e) == EVLIST_INIT)
    {
    	if (e->ev_arg != NULL)
    	{
    		zval_dtor(((php_event*)(e->ev_arg))->cb);
    		FREE_ZVAL(((php_event*)(e->ev_arg))->cb);
    		FREE_ZVAL(((php_event*)(e->ev_arg))->flags);
    		if (IS_RESOURCE == Z_TYPE_P(((php_event*)(e->ev_arg))->stream))
    			zend_list_delete( Z_RESVAL_P(((php_event*)(e->ev_arg))->stream) );
    		free(e->ev_arg);
    	}
    	// стандартное удаление libevent
    	event_del(e);
    } 
    free(e);
}

/*
 * Стандартный callback для вызова из php-кода
 */
void php_event_callback_handler(int fd, short ev, void *arg)
{
	php_event *event = (php_event*)arg;
#ifdef ZTS
	TSRMLS_D = event->TSRMLS_C;
#endif
	zval **params[3];
	zval *retval = NULL;
 	
 	//php_debug_zval_dump(&(event->event));
 	
 	params[0] = &(event->stream);
	params[1] = &(event->flags);
	params[2] = &(event->event);
	
	/* callback в php */
	if (call_user_function_ex(EG(function_table), NULL, event->cb, &retval, 3, params, 0, NULL TSRMLS_CC) == FAILURE)
	{
		 php_error_docref(NULL TSRMLS_CC, E_ERROR, "Callback failed");
	}
	
	if (retval) 
    	zval_ptr_dtor(&retval);
	
	//zend_list_delete(php_stream_get_resource_id(event->stream));
	return;
}


PHP_FUNCTION(event_set)
{
	struct event *e;
    char *callable;
    int event_flags;
    int fd;
    zval *e_res, *z_cb, *z_stream;
    php_stream *stream;
    php_event *event_args;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rzlz", &e_res, &z_stream, &event_flags, &z_cb) == FAILURE) 
    {
        RETURN_FALSE;
    }
    
    /* Извлекаем event структуру */
    e = (struct event*) zend_fetch_resource(&e_res TSRMLS_CC, -1, PHP_EVENT_RES_NAME, NULL, 1, le_event);
    if (e == NULL) 
    {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "First argument must be a valid event resource");
        RETURN_FALSE;
    }
    /* Проверям, что пришло - сигнал или "поток" */
    if (IS_LONG == Z_TYPE_P(z_stream))
    {
    	/* отловили сигнал, z_stream щас содержит номер сигнала */
    	fd = Z_LVAL_P(z_stream);
    }
    else
    {	
	    /* извлекаем "поток" */
	    php_stream_from_zval(stream, &z_stream);
	    if (!stream)
	    {
	    	php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid stream");
	    	RETURN_FALSE;
	    }
	    if (FAILURE == php_stream_cast(stream, PHP_STREAM_AS_FD_FOR_SELECT, (void*)&fd, 1) || fd == -1)
	    {
	    	php_error_docref(NULL TSRMLS_CC, E_WARNING, "Incompatible stream");
	    	RETURN_FALSE;
	    }
	    /* устанавливаем неблокируемые и небуфиризируемые "потоки"*/
	    php_stream_set_option(stream, PHP_STREAM_OPTION_BLOCKING, 0, NULL);
	    stream->flags |= PHP_STREAM_FLAG_NO_BUFFER;
    }
    
#ifdef ZEND_ENGINE_2
    if (!zend_make_callable(z_cb, &callable TSRMLS_CC))
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid callback: '%s'", callable);
        return;
    }
#endif
    
    efree(callable);

    event_args = (php_event*)malloc(sizeof(php_event));
    
    event_args->cb = z_cb;
    Z_ADDREF_P(event_args->cb);
    
    MAKE_STD_ZVAL(event_args->flags);
    ZVAL_LONG(event_args->flags, event_flags);
    
    event_args->stream = z_stream;
    event_args->event = e_res;
    
    // 23.05.2011  - добавление возможности указывать в callback функции параметров, через массив
    
    //event_args->agr = args;

    event_set(e, fd, event_flags, php_event_callback_handler, (void*)event_args);
    
    RETURN_TRUE;
}

PHP_FUNCTION(event_add)
{
	zval *e_res, *z_sec, *z_usec;
	struct event *e;
	struct timeval timeout;
	int ret;
	int argc;

	argc = ZEND_NUM_ARGS();

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|zz", &e_res, &z_sec, &z_usec) == FAILURE) 
    {
        RETURN_FALSE;
    }
    e = (struct event*) zend_fetch_resource(&e_res TSRMLS_CC, -1, PHP_EVENT_RES_NAME, NULL, 1, le_event);
    if (e == NULL) 
    {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "First argument must be a valid event resource");
        RETURN_FALSE;
    }
    
    /* Добавляем событие в планировщик, щас тупо по времени */
    
    timeout.tv_sec = (argc > 1 && Z_TYPE_P(z_sec) == IS_LONG) ? Z_LVAL_P(z_sec) : 0;
    timeout.tv_usec = (argc > 2 && Z_TYPE_P(z_usec) == IS_LONG) ? Z_LVAL_P(z_usec) : 0;
    
    
    /*
    timeout.tv_sec = 0L;
    timeout.tv_usec = 0; 
    if (argc > 1)
    {
          timeout.tv_sec = Z_LVAL_P(z_sec);
          timeout.tv_usec = Z_LVAL_P(z_usec);  
    }
    */
    
    
    //php_printf("sec %d, usec %d, usec: %d\n", timeout.tv_sec, timeout.tv_usec, Z_LVAL_P(z_usec));            
    
    if (argc > 1){
    	ret = event_add(e, &timeout);
     }
    else{
    	ret = event_add(e, NULL);
     }
    /* Проверка на правильность установки */
    if (ret == 0)
    {
    	RETURN_TRUE;
    }
    else
    {
    	RETURN_FALSE;
    }

}

PHP_FUNCTION(event_del)
{
	zval *e_res;
	struct event *e;
	
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &e_res) == FAILURE) 
    {
        RETURN_FALSE;
    }
    e = (struct event*) zend_fetch_resource(&e_res TSRMLS_CC, -1, PHP_EVENT_RES_NAME, NULL, 1, le_event);
    if (e == NULL) 
    {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "First argument must be a valid event resource");
        RETURN_FALSE;
    }

    if (event_del(e) == 0)
    {
    	RETURN_TRUE;
    }
    else
    {
    	RETURN_FALSE;
    }

}

PHP_FUNCTION(event_free)
{
	zval *e_res;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &e_res) == FAILURE) 
    {
        RETURN_FALSE;
    }

    zval_dtor(e_res);
}


PHP_FUNCTION(evhttp_response_set)
{
    long http_code;
	char *content, *http_message;
	int content_len, http_message_len;
	evhttp_response *response;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sls", &content, &content_len, &http_code, &http_message, &http_message_len) == FAILURE)
	{
	    RETURN_FALSE;
	}

	/*TODO: рассмотреть evbuffer, чтобы не гонять как строку!*/
	response = malloc(sizeof(evhttp_response));
	response->res_code = http_code;
	response->res_message = strdup(http_message);	
	response->res_body_len = content_len;
	
	
	/*
	FILE *fp;
	fp = fopen("php_data2", "wb");
	char buf[200] = {0};
	sprintf(buf, "data reading len: %lu\n", content_len);
	fwrite (buf , 1 , sizeof(buf) , fp );
	fclose(fp);
    */	
	
	if (content_len > 0) 
	{
	    response->res_body = malloc(content_len+1);
	    memcpy(response->res_body, content, content_len);
	}       

	ZEND_REGISTER_RESOURCE(return_value, response, le_evhttp_response);
}

/*
 * Обраточик callback 
 */
void php_callback_handler(struct evhttp_request *req, void *arg)  
{  
    zval *cb;
    zval *retval = NULL;
    zval **params[1];
    zval *req_resource;
    cb = ((evhttp_callback_arg *)arg)->arg;
    int res;
    struct evbuffer *buf;
    evhttp_response *response;
    char *str;
    int str_len;
    int res_id; 
    void *retval_res;
    int retval_res_type;
#ifdef ZTS
	TSRMLS_D = ((evhttp_callback_arg *) arg)->TSRMLS_C;
#endif


    /* Передача запроса в php структуру */
    MAKE_STD_ZVAL(req_resource);
    res_id = zend_register_resource(req_resource, req, le_evhttp_request);
    params[0] = &req_resource;

    res = call_user_function_ex(EG(function_table), NULL, cb, &retval, 1, params, 0, NULL TSRMLS_CC);

    req->cb_arg = NULL;
    zend_list_delete(res_id);
    FREE_ZVAL(req_resource);

    if (!retval) 
    {
        /* Ничего не пришло от callback функции, где-то жопа случилась */
        zend_throw_exception(zend_exception_get_default(TSRMLS_C), "Prepare to 503 ERR: Request callback returned nothing", 0 TSRMLS_CC);
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Request callback returned nothing");
        buf = evbuffer_new();
        evhttp_send_reply(req, HTTP_SERVUNAVAIL, "ERR", buf);
        return;
    }

    switch (retval->type)
    {
       case IS_RESOURCE:
             /*
              *  Тупо все, что вернулось, далее смотрим что это :-)
              */
            retval_res = zend_list_find(Z_RESVAL_P(retval), &retval_res_type);
            if (retval_res_type == le_evbuffer)
            {
                buf = (struct evbuffer*) retval_res;
                /* Получили evbuffer обратно, и фигачим его в "браузер "*/
                evhttp_send_reply(req, HTTP_OK, "OK", buf);
            } else if (retval_res_type == le_evhttp_response) {
                /* получаем ресурс ответа назад */
                response = (evhttp_response *) retval_res;
                buf = evbuffer_new();
                if (response->res_body_len > 0)
                {
                    evbuffer_add(buf, response->res_body, response->res_body_len);
                }
                evhttp_send_reply(req, response->res_code, response->res_message, buf);
            } else {
                /* Битый или неправильный тип ресурса */
                php_error_docref(NULL TSRMLS_CC, E_WARNING, "Request callback returned illegal resource");
                buf = evbuffer_new();
                evhttp_send_reply(req, HTTP_SERVUNAVAIL, "ERR", buf);
            }
            break;

       case IS_STRING:
           /* строка */
           str = Z_STRVAL_P(retval);
           str_len = Z_STRLEN_P(retval);
           buf = evbuffer_new();
           evbuffer_add(buf, str, str_len);
           evhttp_send_reply(req, HTTP_OK, "OK", buf);
           break;

       default:
           /* Ничего не пришло от callback*/
           php_error_docref(NULL TSRMLS_CC, E_WARNING, "Request callback returned wrong datatype");
           buf = evbuffer_new();
           evhttp_send_reply(req, HTTP_OK, "OK", buf);
    }
    
    zval_ptr_dtor(&retval);
    evbuffer_free(buf);
    return;
}

PHP_FUNCTION(evhttp_start)
{
    struct evhttp *httpd;
    char *listen_ip;
    int port;
    int listen_ip_len;
    int timeout = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl|l", &listen_ip, &listen_ip_len, &port, &timeout) == FAILURE) 
    {
        RETURN_NULL();
    }

    // устаревшая функция!
    httpd = evhttp_start(listen_ip, port);

    // обвчно, если поые или порт уже забинден - ошибка.
    if (!httpd) 
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING,
                "Error binding server on %s port %d", listen_ip, port);
        RETURN_FALSE;
    }

    if (timeout == 0){
       timeout = 10;
    }

    evhttp_set_timeout(httpd, timeout);
    
    ZEND_REGISTER_RESOURCE(return_value, httpd, le_evhttp);
}


PHP_FUNCTION(evhttp_set_gencb)
{
    struct evhttp *httpd;
    zval *res_httpd, *php_cb;
    zval *cb;
    char *callable = NULL;
	evhttp_callback_arg *cb_arg = (evhttp_callback_arg *) emalloc(sizeof(evhttp_callback_arg));

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz", &res_httpd, &php_cb) == FAILURE)
    {
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE(httpd, struct evhttp*, &res_httpd, -1, PHP_EVHTTP_RES_NAME, le_evhttp);
    
#ifdef ZEND_ENGINE_2
    if (!zend_make_callable(php_cb, &callable TSRMLS_CC))
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "function '%s' is not a valid callback", callable);
        efree(callable);
        return;
    }
#endif
    
    MAKE_STD_ZVAL(cb_arg->arg);
    *(cb_arg->arg) = *php_cb;
    zval_copy_ctor(cb_arg->arg);  
#ifdef ZTS
	cb_arg->TSRMLS_C = TSRMLS_C;
#endif

    evhttp_set_gencb(httpd, php_callback_handler, (void *)cb_arg);
}


PHP_FUNCTION(evhttp_request_get_uri)
{
    struct evhttp_request *req;
    zval *res_req;
    zval *z_uri;
    const char *uri;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &res_req) == FAILURE)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not fetch resource");
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE(req, struct evhttp_request*, &res_req, -1, PHP_EVHTTP_REQUEST_RES_NAME, le_evhttp_request);
    uri = evhttp_request_uri(req);
    
    ZVAL_STRING(return_value, (char*)uri, 1);
    return; 
}

PHP_FUNCTION(evhttp_request_method)
{
        struct evhttp_request *req;
        zval *res_req;
        
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &res_req) == FAILURE)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not fetch resource");
        RETURN_FALSE;
    }

        ZEND_FETCH_RESOURCE(req, struct evhttp_request*, &res_req, -1, PHP_EVHTTP_REQUEST_RES_NAME, le_evhttp_request);

        switch (req->type) {
                case EVHTTP_REQ_GET:
                        RETURN_STRING("GET", 1);
                case EVHTTP_REQ_POST:
                        RETURN_STRING("POST", 1);
                case EVHTTP_REQ_HEAD:
                        RETURN_STRING("HEAD", 1);
                        //                   Если надо будет что-то из REST
/*
                case EVHTTP_REQ_OPTIONS:
                        RETURN_STRING("OPTIONS", 1);                                        
                  case EVHTTP_REQ_PUT:
                        RETURN_STRING("PUT", 1);
                case EVHTTP_REQ_DELETE:
                        RETURN_STRING("DELETE", 1);
                case EVHTTP_REQ_OPTIONS:
                        RETURN_STRING("OPTIONS", 1);
                case EVHTTP_REQ_TRACE:
                        RETURN_STRING("TRACE", 1);
                case EVHTTP_REQ_CONNECT:
                        RETURN_STRING("CONNECT", 1); */
                default:
                        RETURN_NULL();
        }
}

PHP_FUNCTION(evhttp_request_find_header)
{
    struct evhttp_request *req;
    zval *res_req;
    const char *headervalue;
    char *headername;
    int headername_len;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &res_req, &headername, &headername_len) == FAILURE)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not fetch resource");
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE(req, struct evhttp_request*, &res_req, -1, PHP_EVHTTP_REQUEST_RES_NAME, le_evhttp_request);
    if ((headervalue = evhttp_find_header(req->input_headers, headername)) == NULL)
    {
    	/* не нашли заголовка! */
    	RETURN_NULL();
    }
    
    ZVAL_STRING(return_value, (char*)headervalue, 1);
    return; 
}

PHP_FUNCTION(evhttp_request_headers)
{
    struct evhttp_request *req;
    zval *res_req;
    struct evkeyval *header;
	struct evkeyvalq *q;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &res_req) == FAILURE)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not fetch resource");
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE(req, struct evhttp_request*, &res_req, -1, PHP_EVHTTP_REQUEST_RES_NAME, le_evhttp_request);
    
	array_init(return_value);
	q = req->input_headers;
	
    TAILQ_FOREACH (header, q, next)
	{
		add_assoc_string(return_value, header->key, header->value, TRUE);
	}
    return; 
}

PHP_FUNCTION(evhttp_request_add_header)
{
    struct evhttp_request *req;
    zval *res_req;
    char *headername, *headervalue;
    int headername_len, headervalue_len;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rss", &res_req, &headername, &headername_len, &headervalue, &headervalue_len) == FAILURE)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "error reading parameters");
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE(req, struct evhttp_request*, &res_req, -1, PHP_EVHTTP_REQUEST_RES_NAME, le_evhttp_request);
    if (evhttp_add_header(req->input_headers, headername, headervalue) != 0)
    {
    	RETURN_FALSE;
    }
    
    RETURN_TRUE; 
}

PHP_FUNCTION(evhttp_response_add_header)
{
    struct evhttp_request *req;
    zval *res_req;
    char *headername, *headervalue;
    int headername_len, headervalue_len;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rss", &res_req, &headername, &headername_len, &headervalue, &headervalue_len) == FAILURE)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "error reading parameters");
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE(req, struct evhttp_request*, &res_req, -1, PHP_EVHTTP_REQUEST_RES_NAME, le_evhttp_request);

    if (evhttp_add_header(req->output_headers, headername, headervalue) != 0)
    {
    	RETURN_FALSE;
    }
    
    RETURN_TRUE; 
}


PHP_FUNCTION(evhttp_request_status)
{
    struct evhttp_request *req;
    zval *res_req;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &res_req) == FAILURE)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "error reading parameters");
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE(req, struct evhttp_request*, &res_req, -1, PHP_EVHTTP_REQUEST_RES_NAME, le_evhttp_request);
    RETURN_LONG(req->response_code); 
}


PHP_FUNCTION(evhttp_request_input_buffer)
{
    struct evhttp_request *req;
    zval *res_req;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &res_req) == FAILURE)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not fetch resource");
        RETURN_FALSE;
    }
    
    ZEND_FETCH_RESOURCE(req, struct evhttp_request*, &res_req, -1, PHP_EVHTTP_REQUEST_RES_NAME, le_evhttp_request);
    ZEND_REGISTER_RESOURCE(return_value, req->input_buffer, le_evbuffer);
}   

PHP_FUNCTION(evhttp_request_body)
{
    struct evhttp_request *req;
    zval *res_req;
    int body_len, status;
    char *body;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &res_req) == FAILURE)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not fetch resource");
        RETURN_FALSE;
    }
    
    ZEND_FETCH_RESOURCE(req, struct evhttp_request*, &res_req, -1, PHP_EVHTTP_REQUEST_RES_NAME, le_evhttp_request);

	body_len = EVBUFFER_LENGTH(req->input_buffer);
	
	/*
	FILE *fp;
	fp = fopen("php_data", "wb");
	char buf[200] = {0};
	sprintf(buf, "data incoming len: %lu\n", body_len);
	fwrite (buf , 1 , sizeof(buf) , fp );
	fclose(fp);
    */
    
    if (req->input_buffer == NULL || body_len == 0)
    {
    	RETURN_FALSE;
    }
    
    body = emalloc(body_len + 1);
    status = evbuffer_remove(req->input_buffer, (void *) body, body_len + 1);
    body[body_len] = 0;

	if (status == -1) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not get data from resource");
		RETURN_FALSE;
	}
    
    //ZVAL_STRING(return_value, body, 0);
    /*
    ZVAL_STRING нифига не binary-safe, т.е. если мы получаем RAW POST, в котором есть 2-е данные,
    то все, пиздец, данные обрезаются до певого символа x00, что в православном C 
    означает конец строки
    */
       
    ZVAL_STRINGL(return_value, body, body_len, 0);
}   

PHP_FUNCTION(evhttp_request_append_body)
{
    struct evhttp_request *req;
    zval *res_req;
    int body_len;
    char *body;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &res_req, &body, &body_len) == FAILURE)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "error reading params");
        RETURN_FALSE;
    }
    
    ZEND_FETCH_RESOURCE(req, struct evhttp_request*, &res_req, -1, PHP_EVHTTP_REQUEST_RES_NAME, le_evhttp_request);
    
    if (req->output_buffer == NULL)
    {
    	RETURN_FALSE;
    }
    
    if (evbuffer_add(req->output_buffer, body, body_len) != 0)
    {
    	RETURN_FALSE;
    }
    RETURN_TRUE;
}   


PHP_FUNCTION(evbuffer_new)
{
    struct evbuffer *buf;  
    buf = evbuffer_new();  
       
    if (buf == NULL)
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Couldn't create evbuffer.");
        RETURN_FALSE;
    }

    ZEND_REGISTER_RESOURCE(return_value, buf, le_evbuffer);
}

PHP_FUNCTION(evbuffer_free)
{
    struct evbuffer *buf;
    zval *res_buf;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &res_buf) == FAILURE)
    {
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Couldn't free evbuffer");
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE(buf, struct evbuffer*, &res_buf, -1, PHP_EVBUFFER_RES_NAME, le_evbuffer);
    evbuffer_free(buf);
    RETURN_TRUE;
}

PHP_FUNCTION(evbuffer_add)
{
    struct evbuffer *buf;
    zval *res_buf;
    char *str;
    int str_len;
    int added;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &res_buf, &str, &str_len) == FAILURE)
    {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "evbuffer resource and string required");
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE(buf, struct evbuffer*, &res_buf, -1, PHP_EVBUFFER_RES_NAME, le_evbuffer);
    added = evbuffer_add(buf, str, str_len);
    RETVAL_LONG(added);
    return;
}

PHP_FUNCTION(evbuffer_readline)
{
    struct evbuffer *buf;
    zval *res_buf;
    char *line;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &res_buf) == FAILURE)
    {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "evbuffer resource required");
        RETURN_FALSE;
    }

    ZEND_FETCH_RESOURCE(buf, struct evbuffer*, &res_buf, -1, PHP_EVBUFFER_RES_NAME, le_evbuffer);
    line = evbuffer_readline(buf);
	if (line == 0)
	{
		RETURN_FALSE;
	}
	else
	{
		ZVAL_STRING(return_value, line, 1);
	}
}


/**
 * Вызываем libevent если данные уже прочитаны
 */
void callback_buffered_on_read(struct bufferevent *bev, void *arg)
{
	php_bufferevent *event = (php_bufferevent*)arg;
#ifdef ZTS
	TSRMLS_D = event->TSRMLS_C;
#endif
	zval **params[1];
	zval *retval = NULL;
 	
	params[0] = &(event->res_bufferevent);
	
	/* callback в php */
	if (call_user_function_ex(EG(function_table), NULL, event->r_cb, &retval, 1, params, 0, NULL TSRMLS_CC) == FAILURE)
	{
		 php_error_docref(NULL TSRMLS_CC, E_ERROR, "Callback failed");
	}
	
	if (retval)
    	zval_ptr_dtor(&retval);
	
	return;
}

/**
 * Если в буфере записи 0б то вызваем libevent
 * Она жзает это, но мы не используем!
 */
void callback_buffered_on_write(struct bufferevent *bev, void *arg)
{
	php_bufferevent *event = (php_bufferevent*)arg;
#ifdef ZTS
	TSRMLS_D = event->TSRMLS_C;
#endif
	zval **params[1];
	zval *retval = NULL;
 	
	params[0] = &(event->res_bufferevent);
	
	/* callback в php */
	if (call_user_function_ex(EG(function_table), NULL, event->w_cb, &retval, 1, params, 0, NULL TSRMLS_CC) == FAILURE)
	{
		 php_error_docref(NULL TSRMLS_CC, E_ERROR, "Callback failed");
	}
	
	if (retval) 
    	zval_ptr_dtor(&retval);
	
	return;	
}

/**
 * Если ошибка сокета, то вызываем Libevent
 */
void callback_buffered_on_error(struct bufferevent *bev, short what, void *arg)
{
	php_bufferevent *event = (php_bufferevent*)arg;
#ifdef ZTS
	TSRMLS_D = event->TSRMLS_C;
#endif
	zval **params[3];
	zval *code;
	zval *retval = NULL;
	
 	params[0] = &(event->stream);
	params[1] = &(event->res_bufferevent);
	
	MAKE_STD_ZVAL(code);
	ZVAL_LONG(code, what);
	params[2] = &code;

	if (call_user_function_ex(EG(function_table), NULL, event->e_cb, &retval, 3, params, 0, NULL TSRMLS_CC) == FAILURE)
	{
		 php_error_docref(NULL TSRMLS_CC, E_ERROR, "Callback failed");
	}
	
	if (retval) 
    	zval_ptr_dtor(&retval);
	
	zval_ptr_dtor(&code); 
	
	return;
}


PHP_FUNCTION(bufferevent_new)
{
	struct bufferevent *e;
	char *r_callable, *w_callable, *e_callable;
    int fd;
    zval *z_rcb, *z_wcb, *z_ecb, *z_stream;
    php_stream *stream;
    php_bufferevent *be;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzzz", &z_stream, &z_rcb, &z_wcb, &z_ecb) == FAILURE) 
    {
        RETURN_FALSE;
    }

    php_stream_from_zval(stream, &z_stream);
    if (!stream)
    {
    	php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid stream");
    	RETURN_FALSE;
    }
    if (FAILURE == php_stream_cast(stream, PHP_STREAM_AS_FD_FOR_SELECT, (void*)&fd, 1) || fd == -1)
    {
    	php_error_docref(NULL TSRMLS_CC, E_WARNING, "Incompatible stream");
    	RETURN_FALSE;
    }
    
    php_stream_set_option(stream, PHP_STREAM_OPTION_BLOCKING, 0, NULL);
    stream->flags |= PHP_STREAM_FLAG_NO_BUFFER;
    
#ifdef ZEND_ENGINE_2
    if (!zend_make_callable(z_rcb, &r_callable TSRMLS_CC))
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid callback: '%s'", r_callable);
        return;
    }
    if (!zend_make_callable(z_wcb, &w_callable TSRMLS_CC))
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid callback: '%s'", w_callable);
        return;
    }
    if (!zend_make_callable(z_ecb, &e_callable TSRMLS_CC))
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid callback: '%s'", e_callable);
        return;
    }
#endif
    
    efree(r_callable);
    efree(w_callable);
    efree(e_callable);
    
    /* резервирем память для структруы bifferevent */
    if (!(be = malloc(sizeof(php_bufferevent))))
    {
     	php_error_docref(NULL TSRMLS_CC, E_ERROR, "Could not create php_bufferevent internal struct");
       	RETURN_FALSE;
    }
    
    /*  Создаем структуру с callback'ами, "потоками" и событиями */
    be->r_cb = z_rcb;
    be->w_cb = z_wcb;
    be->e_cb = z_ecb;
    Z_ADDREF_P(be->r_cb);
    Z_ADDREF_P(be->w_cb);
    Z_ADDREF_P(be->e_cb);

    /* пинаем libevent */
    if (NULL == (e = bufferevent_new(fd, callback_buffered_on_read, callback_buffered_on_write, callback_buffered_on_error, be)))
    {
        Z_DELREF_P(be->r_cb);
        Z_DELREF_P(be->w_cb);
        Z_DELREF_P(be->e_cb);
    	free(be);
    	php_error_docref(NULL TSRMLS_CC, E_ERROR, "Could not create bufferevent");
    	RETURN_FALSE;
    }
    
    be->stream = z_stream;
    Z_ADDREF_P(be->stream);
    
    MAKE_STD_ZVAL(be->res_bufferevent);
    ZEND_REGISTER_RESOURCE(be->res_bufferevent, e, le_bufferevent);
    RETVAL_ZVAL(be->res_bufferevent, 0, 0);
}

PHP_FUNCTION(bufferevent_enable)
{
	zval *e_res;
	struct bufferevent *e;
	int flags;
	
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &e_res, &flags) == FAILURE) 
    {
        RETURN_FALSE;
    }

    e = (struct bufferevent*) zend_fetch_resource(&e_res TSRMLS_CC, -1, PHP_BUFFEREVENT_RES_NAME, NULL, 1, le_bufferevent);
    if (e == NULL) 
    {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "First argument must be a valid bufferevent resource");
        RETURN_FALSE;
    }

    if (bufferevent_enable(e, flags) == 0)
    {
    	RETURN_TRUE;
    }
    else
    {
    	RETURN_FALSE;
    }

}

PHP_FUNCTION(bufferevent_disable)
{
	zval *e_res;
	struct bufferevent *e;
	int flags;
	
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl", &e_res, &flags) == FAILURE) 
    {
        RETURN_FALSE;
    }

    e = (struct bufferevent*) zend_fetch_resource(&e_res TSRMLS_CC, -1, PHP_BUFFEREVENT_RES_NAME, NULL, 1, le_bufferevent);
    if (e == NULL) 
    {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "First argument must be a valid bufferevent resource");
        RETURN_FALSE;
    }

    if (bufferevent_disable(e, flags) == 0)
    {
    	RETURN_TRUE;
    }
    else
    {
    	RETURN_FALSE;
    }
}


PHP_FUNCTION(bufferevent_read)
{
	zval *target, *e_res;
	struct bufferevent *e;
	int count, bytesread;
	
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzl", &e_res, &target, &count) == FAILURE) 
    {
        RETURN_FALSE;
    }
    
    if (!PZVAL_IS_REF(target) || count < 1)
    {
        RETURN_FALSE;
    }

    e = (struct bufferevent*) zend_fetch_resource(&e_res TSRMLS_CC, -1, PHP_BUFFEREVENT_RES_NAME, NULL, 1, le_bufferevent);
    if (e == NULL) 
    {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "First argument must be a valid bufferevent resource");
        RETURN_FALSE;
    }
        
    convert_to_string(target);

    Z_STRVAL_P(target) = erealloc(Z_STRVAL_P(target), Z_STRLEN_P(target) + count + 1);
    bytesread = bufferevent_read(e, Z_STRVAL_P(target) + Z_STRLEN_P(target), count + 1);
    
    if (bytesread < count)
    {
    	/* TODO: очень надо переписать перерспредление памяти*/
    	Z_STRVAL_P(target) = erealloc(Z_STRVAL_P(target), Z_STRLEN_P(target) + bytesread + 1);
    	count = bytesread;
    }
    
    Z_STRLEN_P(target) += count;
    *(Z_STRVAL_P(target) + Z_STRLEN_P(target)) = 0;
    RETURN_LONG(count);
}


PHP_FUNCTION(bufferevent_write)
{
	zval *e_res;
	struct bufferevent *e;
	char *data;
	int count;
	
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zs", &e_res, &data, &count) == FAILURE) 
    {
        RETURN_FALSE;
    }

    e = (struct bufferevent*) zend_fetch_resource(&e_res TSRMLS_CC, -1, PHP_BUFFEREVENT_RES_NAME, NULL, 1, le_bufferevent);
    if (e == NULL) 
    {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "First argument must be a valid bufferevent resource");
        RETURN_FALSE;
    }

    if (bufferevent_write(e, data, count) != 0)
    {
    	RETURN_FALSE;
    }
    
    RETURN_TRUE;
}


PHP_FUNCTION(ntohs)
{
	char *in;
	int in_len;

	if ((zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &in, &in_len) == FAILURE) || (in_len != 2))
	{
	    php_error_docref(NULL TSRMLS_CC, E_WARNING, "parameter expected to be a 2-byte string in network byte order");
	    RETURN_FALSE;
	}

	RETVAL_DOUBLE(ntohs(*((uint16_t*)in)));
	return;
}


PHP_FUNCTION(ntohl)
{
	char *in;
	int in_len;
	

	if ((zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &in, &in_len) == FAILURE) || (in_len != 4))
	{
	    php_error_docref(NULL TSRMLS_CC, E_WARNING, "parameter expected to be a 4-byte string in network byte order");
	    RETURN_FALSE;
	}

	RETVAL_DOUBLE(ntohl(*((uint32_t*)in)));
	return;
}


PHP_FUNCTION(htons)
{
	char *in;
	int in_len;

	if ((zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &in, &in_len) == FAILURE) || (in_len != 2))
	{
	    php_error_docref(NULL TSRMLS_CC, E_WARNING, "parameter expected to be a 4-byte string in host byte order");
	    RETURN_FALSE;
	}

	RETVAL_DOUBLE(htons(*((uint16_t*)in)));
	return;
	
}


PHP_FUNCTION(htonl)
{
	char *in;
	int in_len;

	if ((zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &in, &in_len) == FAILURE) || (in_len != 4))
	{
	    php_error_docref(NULL TSRMLS_CC, E_WARNING, "parameter expected to be a 4-byte string in host byte order");
	    RETURN_FALSE;
	}

	RETVAL_DOUBLE(htonl(*((uint32_t*)in)));
	return;
}


PHP_FUNCTION(evhttp_connection_new)
{
	struct evhttp_connection *con;
    char *host_ip;
    int port;
    int host_ip_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &host_ip, &host_ip_len, &port) == FAILURE) 
    {
        RETURN_NULL();
    }

    con = evhttp_connection_new(host_ip, port);


    if (!con) 
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING,
                "Error connection to %s port %d", host_ip, port);
        RETURN_FALSE;
    }
    
    ZEND_REGISTER_RESOURCE(return_value, con, le_evhttp_connection);
}

/**
 * Пинаем libvent когда закрывается соединение
 */
void callback_connection_on_close(struct evhttp_connection *con, void *arg)
{
	php_httpcon *connection = (php_httpcon*)arg;
#ifdef ZTS
	TSRMLS_D = connection->TSRMLS_C;
#endif
	zval **params[1];
	zval *retval = NULL;
 	
	params[0] = &(connection->res_httpcon);
	
	/* callback в php */
	if (call_user_function_ex(EG(function_table), NULL, connection->c_cb, &retval, 1, params, 0, NULL TSRMLS_CC) == FAILURE)
	{
		 php_error_docref(NULL TSRMLS_CC, E_ERROR, "Callback failed");
	}
	
	if (retval)
    	zval_ptr_dtor(&retval);
	
	zval_dtor(connection->c_cb);
	FREE_ZVAL(connection->c_cb);
	FREE_ZVAL(connection->res_httpcon);
	free(connection);
	
	return;
}


PHP_FUNCTION(evhttp_connection_set_closecb)
{
	struct evhttp_connection *c;
	php_httpcon *hc;
	char *c_callable;
    zval *z_ccb, *con_res;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz", &con_res, &z_ccb) == FAILURE) 
    {
        RETURN_FALSE;
    }

    c = (struct evhttp_connection*) zend_fetch_resource(&con_res TSRMLS_CC, -1, PHP_EVHTTP_CONNECTION_RES_NAME, NULL, 1, le_evhttp_connection);
    if (c == NULL) 
    {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "First argument must be a valid evhttp_connection resource");
        RETURN_FALSE;
    }

#ifdef ZEND_ENGINE_2
    if (!zend_make_callable(z_ccb, &c_callable TSRMLS_CC))
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid callback: '%s'", c_callable);
        return;
    }
#endif
    
    efree(c_callable);

    if (!(hc = malloc(sizeof(php_httpcon))))
    {
     	php_error_docref(NULL TSRMLS_CC, E_ERROR, "Could not create php_httpcon internal struct");
       	RETURN_FALSE;
    }
    
    hc->c_cb = z_ccb;
    Z_ADDREF_P(hc->c_cb);
    hc->res_httpcon = con_res;
    Z_ADDREF_P(hc->res_httpcon);

    evhttp_connection_set_closecb(c, callback_connection_on_close, hc);
    
    RETURN_TRUE;
}


/**
 * Пинаем Libevent когда http запрос готов
 */
void callback_request_on_complete(struct evhttp_request *req, void *arg)
{
	php_httpevent *event = (php_httpevent*)arg;
#ifdef ZTS
	TSRMLS_D = event->TSRMLS_C;
#endif
	zval **params[1];
	zval *retval = NULL;
 	
	params[0] = &(event->res_httpevent);
	
	if (call_user_function_ex(EG(function_table), NULL, event->r_cb, &retval, 1, params, 0, NULL TSRMLS_CC) == FAILURE)
	{
		 php_error_docref(NULL TSRMLS_CC, E_ERROR, "Callback failed");
	}
	
	if (retval)
    	zval_ptr_dtor(&retval);
	
	return;
}

PHP_FUNCTION(evhttp_request_new)
{
	struct evhttp_request *r;
	php_httpevent *he;
	char *r_callable;
    zval *z_rcb;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &z_rcb) == FAILURE) 
    {
        RETURN_FALSE;
    }    

#ifdef ZEND_ENGINE_2
    if (!zend_make_callable(z_rcb, &r_callable TSRMLS_CC))
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid callback: '%s'", r_callable);
        return;
    }
#endif
    
    efree(r_callable);

    if (!(he = malloc(sizeof(php_httpevent))))
    {
     	php_error_docref(NULL TSRMLS_CC, E_ERROR, "Could not create php_httpevent internal struct");
       	RETURN_FALSE;
    }
    he->r_cb = z_rcb;
    Z_ADDREF_P(he->r_cb);

    if (NULL == (r = evhttp_request_new(callback_request_on_complete, he)))
    {
        Z_DELREF_P(he->r_cb);
    	free(he);
    	php_error_docref(NULL TSRMLS_CC, E_ERROR, "Could not create httpevent");
    	RETURN_FALSE;
    }
    
    MAKE_STD_ZVAL(he->res_httpevent);
    ZEND_REGISTER_RESOURCE(he->res_httpevent, r, le_evhttp_request);
    RETVAL_ZVAL(he->res_httpevent, 0, 0);
}

PHP_FUNCTION(evhttp_make_request)
{
	zval *con_res, *req_res;
	struct evhttp_connection *con;
	struct evhttp_request *req;
	int type, url_len;
	char *url;
	int ret;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rrls", &con_res, &req_res, &type, &url, &url_len) == FAILURE) 
    {
        RETURN_FALSE;
    }

    con = (struct evhttp_connection*) zend_fetch_resource(&con_res TSRMLS_CC, -1, PHP_EVHTTP_CONNECTION_RES_NAME, NULL, 1, le_evhttp_connection);
    if (con == NULL) 
    {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "First argument must be a valid evhttp_connection resource");
        RETURN_FALSE;
    }

    req = (struct evhttp_request*) zend_fetch_resource(&req_res TSRMLS_CC, -1, PHP_EVHTTP_REQUEST_RES_NAME, NULL, 1, le_evhttp_request);
    if (req == NULL)
    {
    	php_error_docref(NULL TSRMLS_CC, E_ERROR, "First argument must be a valid evhttp_request resource");
    	RETURN_FALSE;
    }

	zend_list_addref(Z_RESVAL_P(req_res));
	//Z_ADDREF_P(req_res);
	
    ret = evhttp_make_request(con, req, type, url);
	//php_printf("%d, %s, %d\n", type, url, ret);
	
    RETURN_LONG(ret);
}


PHP_FUNCTION(evhttp_request_free)
{
	struct evhttp_request *req;
    zval *res_req;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &res_req) == FAILURE) 
    {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "First argument must be a valid evhttp_request resource");
        RETURN_FALSE;
    }

    req = (struct evhttp_request*) zend_fetch_resource(&res_req TSRMLS_CC, -1, PHP_EVHTTP_REQUEST_RES_NAME, NULL, 1, le_evhttp_request);
    if (req == NULL)
    {
    	php_error_docref(NULL TSRMLS_CC, E_ERROR, "First argument must be a valid evhttp_request resource");
    	RETURN_FALSE;
    }

	zend_hash_index_del(&EG(regular_list), Z_RESVAL_P(res_req));
	RETURN_TRUE;
}

