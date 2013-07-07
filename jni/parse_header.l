%option noyywrap
%option never-interactive
%option nodefault
%option nounput
%option reentrant
/* %option stack */
%option extra-type="request_header_t *"

/* CAUTION: option order is important */
%option fast
%option ecs

%x CONTENT_LENGTH
%x transfer_encoding
%x connection

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

(?i:Content-Length): {
    BEGIN(CONTENT_LENGTH);
}
(?i:Transfer-Encoding): {
    BEGIN(transfer_encoding);
}
(?i:Connection): {
    BEGIN(connection);
}
.+ {
    ;
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
    . {
        return TOK_UNKNOWN;
    }
    [[:space:]] {
        ;
    }
    \n {
        BEGIN(INITIAL);
    }
}

%%

/*
 * http://flex.sourceforge.net/manual/
 */
int parse_header(const char *buf, int len, request_header_t *h)
{
    yyscan_t scanner;
    yylex_init_extra(h, &scanner);
    yy_scan_bytes(buf, len, &scanner);
    int ret = yylex(scanner);
    yylex_destroy(scanner);

    return ret;
}
