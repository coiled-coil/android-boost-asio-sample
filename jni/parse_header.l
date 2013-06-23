%option noyywrap
%option never-interactive
%option nodefault
%option nounput
%option reentrant
/* %option stack */

/* CAUTION: option order is important */
%option fast
%option ecs

%x content_length
%x transfer_encoding
%x connection

%{
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

(?i:Content-Length):[[:space:]]* {
    BEGIN(content_length);
    return TOK_CONTENT_LENGTH;
}
(?i:Transfer-Encoding):[[:space:]]* {
    BEGIN(transfer_encoding);
    return TOK_TRANSFER_ENCODING;
}
(?i:Connection):[[:space:]]* {
    BEGIN(connection);
    return TOK_CONNECTION;
}
<content_length>{
    [[:digit:]]+ {
        return TOK_INTEGER;
    }
}
<transfer_encoding>{
    chunked {
        return TOK_CHUNKED;
    }
}
<connection>{
    close {
        return TOK_CLOSE;
    }
    keep-alive {
        return TOK_KEEP_ALIVE;
    }
}
<*>.|\n   return TOK_UNKNOWN;

%%
