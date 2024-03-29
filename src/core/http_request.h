/* ********************************
 * Author:       Zhanglele
 * Description:  http 请求管理
 * create time: 2023.02.05
 ********************************/
#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <stdint.h>

#include "sds.h"
#include "t_list.h"
#include "http_pub.h"
#include "url.h"

typedef struct {
    sds_t msg; /* http请求消息 */
} http_request_t;

int http_request_init(http_request_t *req);
int http_request_release(http_request_t *req);

int http_request_build_msg(uint32_t method, http_request_t *req,
    url_data_t *url_data);

#endif