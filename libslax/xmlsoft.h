/*
 * $Id$
 *
 * Copyright (c) 2010, Juniper Networks, Inc.
 * All rights reserved.
 * See ../Copyright for the status of this software
 *
 * This file includes hooks and additions to the libxml2 and libxslt APIs.
 */

#include <libxslt/xsltutils.h>	/* For xsltHandleDebuggerCallback, etc */
#include <libxslt/extensions.h>

#define NUM_ARRAY(x)    (sizeof(x)/sizeof(x[0]))

/* ----------------------------------------------------------------------
 * Functions that aren't prototyped in libxml2 headers
 */

/**
 * nodePush:
 * @ctxt:  an XML parser context
 * @value:  the element node
 *
 * Pushes a new element node on top of the node stack
 *
 * Returns 0 in case of error, the index in the stack otherwise
 */
int
nodePush(xmlParserCtxtPtr ctxt, xmlNodePtr value);

/**
 * nodePop:
 * @ctxt: an XML parser context
 *
 * Pops the top element node from the node stack
 *
 * Returns the node just removed
 */
xmlNodePtr
nodePop(xmlParserCtxtPtr ctxt);

/**
 * Call xsltSetDebuggerCallbacks() with the properly typed argument.
 * I'm not sure of the reason but the arguments are untyped, giving it
 * a significant "ick" factor.  We preserve our dignity by hiding behind
 * an inline.
 * @see http://mail.gnome.org/archives/xslt/2010-September/msg00013.html
 */
static inline void
xsltSetDebuggerCallbacksHelper (xsltHandleDebuggerCallback handler,
				xsltAddCallCallback add_frame,
				xsltDropCallCallback drop_frame)
{
    void *cb[] = { handler, add_frame, drop_frame };

    xsltSetDebuggerCallbacks(NUM_ARRAY(cb), &cb);
}

/*
 * Free a chunk of memory
 */
static inline void
xmlFreeAndEasy (void *ptr)
{
    if (ptr)
	xmlFree(ptr);
}

/*
 * The stock xmlStrchr() returns a "const" pointer, which isn't good.
 */
static inline xmlChar *
xmlStrchru (xmlChar *str, xmlChar val)
{
    for ( ; str && *str != 0; str++)
        if (*str == val)
	    return str;

    return NULL;
}

static inline void
slaxRegisterFunction (const char *uri, const char *fn, xmlXPathFunction func)
{
    xsltRegisterExtModuleFunction((const xmlChar *) fn, (const xmlChar *) uri,
				  func);
}

static inline void
slaxUnregisterFunction (const char *uri, const char *fn)
{
    xsltUnregisterExtModuleFunction((const xmlChar *) fn,
				    (const xmlChar *) uri);
}

typedef struct slax_function_table_s {
    const char *ft_name;	/* Name of the function */
    xmlXPathFunction ft_func;	/* Function pointer */
} slax_function_table_t;

static inline void
slaxRegisterFunctionTable (const char *uri, slax_function_table_t *ftp)
{
    if (ftp)
	for ( ; ftp->ft_name; ftp++)
	    slaxRegisterFunction(uri, ftp->ft_name, ftp->ft_func);
}

static inline void
slaxUnregisterFunctionTable (const char *uri, slax_function_table_t *ftp)
{
    if (ftp)
	for ( ; ftp->ft_name; ftp++)
	    slaxUnregisterFunction(uri, ftp->ft_name);
}

static inline void
slaxRegisterElement (const char *uri, const char *fn, 
		     xsltPreComputeFunction fcompile,
		     xsltTransformFunction felement)
{
    xsltRegisterExtModuleElement((const xmlChar *) fn, (const xmlChar *) uri,
				 fcompile, felement);
}

static inline void
slaxUnregisterElement (const char *uri, const char *fn)
{
    xsltUnregisterExtModuleElement((const xmlChar *) fn,
				   (const xmlChar *) uri);
}

typedef struct slax_element_table_s {
    const char *et_name;	/* Name of the element */
    xsltPreComputeFunction et_fcompile;
    xsltTransformFunction et_felement;
} slax_element_table_t;

static inline void
slaxRegisterElementTable (const char *uri, slax_element_table_t *etp)
{
    if (etp)
	for ( ; etp->et_name; etp++)
	    slaxRegisterElement(uri, etp->et_name,
				etp->et_fcompile, etp->et_felement);
}

static inline void
slaxUnregisterElementTable (const char *uri, slax_element_table_t *etp)
{
    if (etp)
	for ( ; etp->et_name; etp++)
	    slaxUnregisterElement(uri, etp->et_name);
}

static inline int
xmlAtoi (const xmlChar *nptr)
{
    return atoi((const char *) nptr);
}

static inline char *
xmlStrdup2 (const char *str)
{
    return (char *) xmlStrdup((const xmlChar *) str);
}

/*
 * There's no clear way to stop the XSLT engine in the middle of
 * a script, but this is the way <xsl:message terminate="yes">
 * does it.  See xsltMessage().
 */
static inline void
xsltStopEngine (xsltTransformContextPtr ctxt)
{
    if (ctxt)
	ctxt->state = XSLT_STATE_STOPPED;
}

static inline xmlNodePtr
xmlAddChildContent (xmlDocPtr docp, xmlNodePtr parent,
		    const xmlChar *name, const xmlChar *value)
{
    xmlNodePtr nodep = xmlNewDocRawNode(docp, NULL, name, value);
    if (nodep) {
	xmlAddChild(parent, nodep);
    }

    return nodep;
}
