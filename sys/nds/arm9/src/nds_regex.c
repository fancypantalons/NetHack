#include "hack.h"

#include <regex.h>

/*
 *
 * This is a total ripoff of posixregex.c, with the exception that extended
 * regular expressions are *disabled* in this version of the engine.
 *
 * Why?
 *
 * Well, PCRE on the NDS seems to have broken extended regex's.  Or something.
 * Regardless, disabling extended regular expressions seems to solve the problem.
 * I'm not arguing.
 */

const char regex_id[] = "pcreregex";

struct nhregex {
    regex_t re;
    int err;
};

struct nhregex *
regex_init()
{
    return (struct nhregex *) alloc(sizeof(struct nhregex));
}

boolean
regex_compile(const char *s, struct nhregex *re)
{
    if (!re)
        return FALSE;
    if ((re->err = regcomp(&re->re, s, REG_NOSUB)))
        return FALSE;
    return TRUE;
}

const char *
regex_error_desc(struct nhregex *re)
{
    static char buf[BUFSZ];

    if (!re || !re->err)
        return (const char *) 0;

    /* FIXME: Using a static buffer here is not ideal, but avoids memory
     * leaks. Consider the allocation more carefully. */
    regerror(re->err, &re->re, buf, BUFSZ);

    return buf;
}

boolean
regex_match(const char *s, struct nhregex *re)
{
    int result;

    if (!re || !s)
        return FALSE;

    if ((result = regexec(&re->re, s, 0, (genericptr_t) 0, 0))) {
        if (result != REG_NOMATCH)
            re->err = result;
        return FALSE;
    }
    return TRUE;
}

void
regex_free(struct nhregex *re)
{
    regfree(&re->re);
    free(re);
}

