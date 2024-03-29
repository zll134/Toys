/* ********************************
 * Author:       Zhanglele
 * Description:  简单动态数组的实现
 * create time: 2022.01.16
 ********************************/

#include "sds.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

#define MEMORY_THRESHOLD (2 * 1024 * 1024)

/* sds的结构：
    |---  sds_hdr_t ---|------     buf  -----------|
    | strlen | freelen |   str    | \0 |   free    |
*/

sds_t sds_new_with_len(const char *init, int len)
{
    sds_hdr_t *sh = (sds_hdr_t *)calloc(1, sizeof(sds_hdr_t) + len + 1);
    if (sh == NULL) {
        return NULL;
    }

    if (init != NULL) {
        memcpy(sh->buf, init, len);
        sh->free = 0;
        sh->len = len;
    } else {
        sh->free = len;
        sh->len = 0;
    }

    sh->buf[sh->len] = '\0';
    return sh->buf;
}

sds_t sds_new(const char *init)
{
    int len = strlen(init);
    return sds_new_with_len(init, len);
}

void sds_free(sds_t obj)
{
    sds_hdr_t *sh = (sds_hdr_t *)(obj - sizeof(sds_hdr_t));
    free(sh);
}

static sds_hdr_t *sds_get_hdr(sds_t obj)
{
    return (sds_hdr_t *)(obj - sizeof(sds_hdr_t));
}

const char *sds_get_string(sds_t obj)
{
    return (const char *)obj;
}

uint32_t sds_get_len(sds_t obj)
{
    if (obj == NULL) {
        return 0;
    }
    sds_hdr_t *sh = (sds_hdr_t *)(obj - sizeof(sds_hdr_t));
    return sh->len;
}

uint32_t sds_get_capcity(sds_t obj)
{
    if (obj == NULL) {
        return 0;
    }
    sds_hdr_t *sh = (sds_hdr_t *)(obj - sizeof(sds_hdr_t));
    return sh->len + sh->free;
}

int sds_find_str(sds_t obj, uint32_t start, uint32_t end, const char *str)
{
    uint32_t len = sds_get_len(obj);
    int str_len = strlen(str);
    if ((start >= end) || (end > len) || (str_len > (end - start))) {
        ERROR("[SDS]Input param is invalid, objcontent %s, start %u, end %u,"
                 " obj len %u, str %s, str_len %d.",
                 (char *)obj, start, end, len, str, str_len);
        return -1;
    }

    const char *obj_str = sds_get_string(obj);
    for (int i = start; i < end - str_len; i++) {
        if (strncmp(obj_str + i, str, str_len) == 0) {
            return i;
        }
    }

    return -1;
}

sds_t sds_substr(sds_t src, int start, int end)
{
    if ((src == NULL) || (start >= end) || (end > sds_get_len(src))) {
        ERROR("[SDS]Input param is invalid when getting substr, src %s, start %u, end %u,",
                 (char *)src, start, end);
        return NULL;
    }
    uint32_t src_len = sds_get_len(src);
    
    sds_t substr = sds_new_with_len(NULL, end - start);
    if (substr == NULL) {
        ERROR("Create substr failed, src %s, start %d, end %d.",
            (char *)src, start, end);
        return NULL;
    }
    (void)memcpy(substr, src + start, end - start);
    return substr;
}

static uint32_t sds_calc_new_space_size(uint32_t needed_space)
{
    /* 内存增长策略：
        1：需要的空间space小于1M时，申请 2 * space的空间。
        2：需要的空间space大于1M是，申请 space + 1M的空间
    */
    if (needed_space < (1 * 1024 * 1024)) {
        return 2 * needed_space;
    } else {
        return needed_space + (1 * 1024 * 1024);
    }
}

static sds_t sds_make_space(sds_t s, uint32_t needed_space)
{
    sds_hdr_t *hdr = sds_get_hdr(s);
    if (hdr->len + hdr->free > needed_space) {
        return s;
    }
    uint32_t new_len = sds_calc_new_space_size(needed_space);
    sds_hdr_t *new_hdr = (sds_hdr_t *)calloc(1, sizeof(*new_hdr) + new_len + 1);
    if (new_hdr == NULL) {
        ERROR("[sds] Malloc failed");
        sds_free(s);
        return NULL;
    }

    new_hdr->len = hdr->len;
    new_hdr->free = new_len - hdr->len;

    sds_t new_obj = (sds_t)(new_hdr + 1);
    (void)strcpy(new_obj, s);
    sds_free(s);
    return new_obj;
}

static sds_t sds_cat_with_len(sds_t s, const char *t, uint32_t len)
{
    if (s == NULL) {
        s = sds_new_with_len(t, len);
        if (s == NULL) {
            ERROR("[sds] Create new sds failed.");
            return NULL;
        }
    }

    sds_hdr_t *hdr = sds_get_hdr(s);
    s = sds_make_space(s, hdr->len + len);
    if (s == NULL) {
        ERROR("[sds] Malloc failed");
        return NULL;
    }

    /* 动态数组已保证目标地址空间是够的 */
    (void)strcpy(s + hdr->len, (char *)t);
    hdr->len = hdr->len + len;
    hdr->free = hdr->free - len;

    return s;
}

sds_t sds_cat(sds_t obj, const char *t)
{
    return sds_cat_with_len(obj, t, strlen(t));
}

sds_t sds_vcat(sds_t obj, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    sds_t s = obj;
    int format_len = strlen(format);
    for (int i = 0; i < format_len; i++) {
        if (format[i] != 's') {
            continue;
        }

        char *next = va_arg(ap, char *);
        s = sds_cat(s, next);
        if (s == NULL) {
            ERROR("Cat next str failed");
            va_end(ap);  
            return NULL;
        }
    }
    va_end(ap);
    return s;
}
