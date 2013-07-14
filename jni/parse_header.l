%option noyywrap
%option never-interactive
%option nodefault
%option nounput
%option reentrant
/* %option stack */
%option extra-type="request_header_t *"
/* %option yylineno */

/* CAUTION: option order is important */
%option fast
%option ecs

%x CONTENT_LENGTH
%x transfer_encoding
%x connection
%x SKIP

%{
#include <parse_header.h>

enum {
    TOK_UNKNOWN,
    TOK_CONTENT_LENGTH,
    TOK_TRANSFER_ENCODING,
    TOK_CONNECTION,
    TOK_CHUNKED,
    TOK_CLOSE,
    TOK_KEEP_ALIVE,
    TOK_INTEGER,
};
%}

%%

\r\n\r\n {
    return 0;
}
(?i:Content-Length): {
    BEGIN(CONTENT_LENGTH);
}
(?i:Transfer-Encoding): {
    BEGIN(transfer_encoding);
}
(?i:Connection): {
    BEGIN(connection);
}
. {
    BEGIN(SKIP);
}

<SKIP>{
    .+ {
        ;
    }
}
<CONTENT_LENGTH>{
    [[:digit:]]+ {
        yyextra->content_length = atoi(yytext);
    }
}
<transfer_encoding>{
    chunked {
        yyextra->chunked = 1;
    }
}
<connection>{
    close {
        yyextra->keep_alive = 0;
    }
    keep-alive {
        yyextra->keep_alive = 1;
    }
}
<*>{
    [ \t\r] {
        ;
    }
    \n {
        BEGIN(INITIAL);
    }
    . {
        ;
        // return TOK_UNKNOWN;
    }
}

%%

/*
 * http://flex.sourceforge.net/manual/
 */
int parse_header(const char *buf, int len, request_header_t *h)
{
    yyscan_t scanner = NULL;
    yylex_init_extra(h, &scanner);
    YY_BUFFER_STATE buf_state = yy_scan_bytes(buf, len, scanner);
    int ret = yylex(scanner);
    yy_delete_buffer(buf_state, scanner);
    yylex_destroy(scanner);

    return ret;
}
