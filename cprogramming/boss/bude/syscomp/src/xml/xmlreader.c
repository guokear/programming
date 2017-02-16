/*
 * xmlreader.c: implements the xmlTextReader streaming node API
 *
 * NOTE: 
 *   XmlTextReader.Normalization Property won't be supported, since
 *     it makes the parser non compliant to the XML recommendation
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

/*
 * TODOs:
 *   - XML Schemas validation
 */
#define IN_LIBXML
#include "libxml.h"

#ifdef LIBXML_READER_ENABLED
#include <string.h> /* for memset() only ! */
#include <stdarg.h>

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <libxml/xmlmemory.h>
#include <libxml/xmlIO.h>
#include <libxml/xmlreader.h>
#include <libxml/parserInternals.h>
#include <libxml/relaxng.h>
#include <libxml/uri.h>
#ifdef LIBXML_XINCLUDE_ENABLED
#include <libxml/xinclude.h>
#endif
#ifdef LIBXML_PATTERN_ENABLED
#include <libxml/pattern.h>
#endif

/* #define DEBUG_CALLBACKS */
/* #define DEBUG_READER */

/**
 * TODO:
 *
 * macro to flag unimplemented blocks
 */
#define TODO 								\
    xmlGenericError(xmlGenericErrorContext,				\
	    "Unimplemented block at %s:%d\n",				\
            __FILE__, __LINE__);

#ifdef DEBUG_READER
#define DUMP_READER xmlTextReaderDebug(reader);
#else
#define DUMP_READER
#endif

#define CHUNK_SIZE 512
/************************************************************************
 *									*
 *	The parser: maps the Text Reader API on top of the existing	*
 *		parsing routines building a tree			*
 *									*
 ************************************************************************/

#define XML_TEXTREADER_INPUT	1
#define XML_TEXTREADER_CTXT	2

typedef enum {
    XML_TEXTREADER_NONE = -1,
    XML_TEXTREADER_START= 0,
    XML_TEXTREADER_ELEMENT= 1,
    XML_TEXTREADER_END= 2,
    XML_TEXTREADER_EMPTY= 3,
    XML_TEXTREADER_BACKTRACK= 4,
    XML_TEXTREADER_DONE= 5,
    XML_TEXTREADER_ERROR= 6
} xmlTextReaderState;

typedef enum {
    XML_TEXTREADER_NOT_VALIDATE = 0,
    XML_TEXTREADER_VALIDATE_DTD = 1,
    XML_TEXTREADER_VALIDATE_RNG = 2
} xmlTextReaderValidate;

struct _xmlTextReader {
    int				mode;	/* the parsing mode */
    xmlDocPtr			doc;    /* when walking an existing doc */
    xmlTextReaderValidate       validate;/* is there any validation */
    int				allocs;	/* what structure were deallocated */
    xmlTextReaderState		state;
    xmlParserCtxtPtr		ctxt;	/* the parser context */
    xmlSAXHandlerPtr		sax;	/* the parser SAX callbacks */
    xmlParserInputBufferPtr	input;	/* the input */
    startElementSAXFunc		startElement;/* initial SAX callbacks */
    endElementSAXFunc		endElement;  /* idem */
    startElementNsSAX2Func	startElementNs;/* idem */
    endElementNsSAX2Func	endElementNs;  /* idem */
    charactersSAXFunc		characters;
    cdataBlockSAXFunc		cdataBlock;
    unsigned int 		base;	/* base of the segment in the input */
    unsigned int 		cur;	/* current position in the input */
    xmlNodePtr			node;	/* current node */
    xmlNodePtr			curnode;/* current attribute node */
    int				depth;  /* depth of the current node */
    xmlNodePtr			faketext;/* fake xmlNs chld */
    int				preserve;/* preserve the resulting document */
    xmlBufferPtr		buffer; /* used to return const xmlChar * */
    xmlDictPtr			dict;	/* the context dictionnary */

    /* entity stack when traversing entities content */
    xmlNodePtr         ent;          /* Current Entity Ref Node */
    int                entNr;        /* Depth of the entities stack */
    int                entMax;       /* Max depth of the entities stack */
    xmlNodePtr        *entTab;       /* array of entities */

    /* error handling */
    xmlTextReaderErrorFunc errorFunc;    /* callback function */
    void                  *errorFuncArg; /* callback function user argument */

#ifdef LIBXML_SCHEMAS_ENABLED
    /* Handling of RelaxNG validation */
    xmlRelaxNGPtr          rngSchemas;	/* The Relax NG schemas */
    xmlRelaxNGValidCtxtPtr rngValidCtxt;/* The Relax NG validation context */
    int                  rngValidErrors;/* The number of errors detected */
    xmlNodePtr             rngFullNode;	/* the node if RNG not progressive */
#endif
#ifdef LIBXML_XINCLUDE_ENABLED
    /* Handling of XInclude processing */
    int                xinclude;	/* is xinclude asked for */
    const xmlChar *    xinclude_name;	/* the xinclude name from dict */
    xmlXIncludeCtxtPtr xincctxt;	/* the xinclude context */
    int                in_xinclude;	/* counts for xinclude */
#endif
#ifdef LIBXML_PATTERN_ENABLED
    int                patternNr;       /* number of preserve patterns */
    int                patternMax;      /* max preserve patterns */
    xmlPatternPtr     *patternTab;      /* array of preserve patterns */
#endif
    int                preserves;	/* level of preserves */
    int                parserFlags;	/* the set of options set */
    /* Structured error handling */
    xmlStructuredErrorFunc sErrorFunc;  /* callback function */
};

#define NODE_IS_EMPTY		0x1
#define NODE_IS_PRESERVED	0x2
#define NODE_IS_SPRESERVED	0x4

/**
 * CONSTSTR:
 *
 * Macro used to return an interned string
 */
#define CONSTSTR(str) xmlDictLookup(reader->dict, (str), -1)
#define CONSTQSTR(p, str) xmlDictQLookup(reader->dict, (p), (str))

static int xmlTextReaderReadTree(xmlTextReaderPtr reader);
static int xmlTextReaderNextTree(xmlTextReaderPtr reader);

/************************************************************************
 *									*
 *	Our own version of the freeing routines as we recycle nodes	*
 *									*
 ************************************************************************/
/**
 * DICT_FREE:
 * @str:  a string
 *
 * Free a string if it is not owned by the "dict" dictionnary in the
 * current scope
 */
#define DICT_FREE(str)						\
	if ((str) && ((!dict) || 				\
	    (xmlDictOwns(dict, (const xmlChar *)(str)) == 0)))	\
	    xmlFree((char *)(str));

static void xmlTextReaderFreeNode(xmlTextReaderPtr reader, xmlNodePtr cur);
static void xmlTextReaderFreeNodeList(xmlTextReaderPtr reader, xmlNodePtr cur);

/**
 * xmlFreeID:
 * @not:  A id
 *
 * Deallocate the memory used by an id definition
 */
static void
xmlFreeID(xmlIDPtr id) {
    xmlDictPtr dict = NULL;

    if (id == NULL) return;

    if (id->doc != NULL)
        dict = id->doc->dict;

    if (id->value != NULL)
	DICT_FREE(id->value)
    xmlFree(id);
}

/**
 * xmlTextReaderRemoveID:
 * @doc:  the document
 * @attr:  the attribute
 *
 * Remove the given attribute from the ID table maintained internally.
 *
 * Returns -1 if the lookup failed and 0 otherwise
 */
static int
xmlTextReaderRemoveID(xmlDocPtr doc, xmlAttrPtr attr) {
    xmlIDTablePtr table;
    xmlIDPtr id;
    xmlChar *ID;

    if (doc == NULL) return(-1);
    if (attr == NULL) return(-1);
    table = (xmlIDTablePtr) doc->ids;
    if (table == NULL) 
        return(-1);

    if (attr == NULL)
	return(-1);
    ID = xmlNodeListGetString(doc, attr->children, 1);
    if (ID == NULL)
	return(-1);
    id = (xmlIDPtr)xmlHashLookup(table, ID);
    xmlFree(ID);
    if (id == NULL || id->attr != attr) {
	return(-1);
    }
    id->name = attr->name;
    id->attr = NULL;
    return(0);
}

/**
 * xmlTextReaderFreeProp:
 * @reader:  the xmlTextReaderPtr used
 * @cur:  the node
 *
 * Free a node.
 */
static void
xmlTextReaderFreeProp(xmlTextReaderPtr reader, xmlAttrPtr cur) {
    xmlDictPtr dict;

    dict = reader->ctxt->dict;
    if (cur == NULL) return;

    if ((__xmlRegisterCallbacks) && (xmlDeregisterNodeDefaultValue))
	xmlDeregisterNodeDefaultValue((xmlNodePtr) cur);

    /* Check for ID removal -> leading to invalid references ! */
    if ((cur->parent != NULL) && (cur->parent->doc != NULL) &&
	((cur->parent->doc->intSubset != NULL) ||
	 (cur->parent->doc->extSubset != NULL))) {
        if (xmlIsID(cur->parent->doc, cur->parent, cur))
	    xmlTextReaderRemoveID(cur->parent->doc, cur);
    }
    if (cur->children != NULL)
        xmlTextReaderFreeNodeList(reader, cur->children);

    DICT_FREE(cur->name);
    if ((reader != NULL) && (reader->ctxt != NULL) &&
        (reader->ctxt->freeAttrsNr < 100)) {
        cur->next = reader->ctxt->freeAttrs;
	reader->ctxt->freeAttrs = cur;
	reader->ctxt->freeAttrsNr++;
    } else {
	xmlFree(cur);
    }
}

/**
 * xmlTextReaderFreePropList:
 * @reader:  the xmlTextReaderPtr used
 * @cur:  the first property in the list
 *
 * Free a property and all its siblings, all the children are freed too.
 */
static void
xmlTextReaderFreePropList(xmlTextReaderPtr reader, xmlAttrPtr cur) {
    xmlAttrPtr next;
    if (cur == NULL) return;
    while (cur != NULL) {
        next = cur->next;
        xmlTextReaderFreeProp(reader, cur);
	cur = next;
    }
}

/**
 * xmlTextReaderFreeNodeList:
 * @reader:  the xmlTextReaderPtr used
 * @cur:  the first node in the list
 *
 * Free a node and all its siblings, this is a recursive behaviour, all
 * the children are freed too.
 */
static void
xmlTextReaderFreeNodeList(xmlTextReaderPtr reader, xmlNodePtr cur) {
    xmlNodePtr next;
    xmlDictPtr dict;

    dict = reader->ctxt->dict;
    if (cur == NULL) return;
    if (cur->type == XML_NAMESPACE_DECL) {
	xmlFreeNsList((xmlNsPtr) cur);
	return;
    }
    if ((cur->type == XML_DOCUMENT_NODE) ||
	(cur->type == XML_HTML_DOCUMENT_NODE)) {
	xmlFreeDoc((xmlDocPtr) cur);
	return;
    }
    while (cur != NULL) {
        next = cur->next;
	/* unroll to speed up freeing the document */
	if (cur->type != XML_DTD_NODE) {

	    if ((cur->children != NULL) &&
		(cur->type != XML_ENTITY_REF_NODE)) {
		if (cur->children->parent == cur)
		    xmlTextReaderFreeNodeList(reader, cur->children);
		cur->children = NULL;
	    }

	    if ((__xmlRegisterCallbacks) && (xmlDeregisterNodeDefaultValue))
		xmlDeregisterNodeDefaultValue(cur);

	    if (((cur->type == XML_ELEMENT_NODE) ||
		 (cur->type == XML_XINCLUDE_START) ||
		 (cur->type == XML_XINCLUDE_END)) &&
		(cur->properties != NULL))
		xmlTextReaderFreePropList(reader, cur->properties);
	    if ((cur->type != XML_ELEMENT_NODE) &&
		(cur->type != XML_XINCLUDE_START) &&
		(cur->type != XML_XINCLUDE_END) &&
		(cur->type != XML_ENTITY_REF_NODE)) {
		DICT_FREE(cur->content);
	    }
	    if (((cur->type == XML_ELEMENT_NODE) ||
	         (cur->type == XML_XINCLUDE_START) ||
		 (cur->type == XML_XINCLUDE_END)) &&
		(cur->nsDef != NULL))
		xmlFreeNsList(cur->nsDef);

	    /*
	     * we don't free element names here they are interned now
	     */
	    if ((cur->type != XML_TEXT_NODE) &&
		(cur->type != XML_COMMENT_NODE))
		DICT_FREE(cur->name);
	    if (((cur->type == XML_ELEMENT_NODE) ||
		 (cur->type == XML_TEXT_NODE)) &&
	        (reader != NULL) && (reader->ctxt != NULL) &&
		(reader->ctxt->freeElemsNr < 100)) {
	        cur->next = reader->ctxt->freeElems;
		reader->ctxt->freeElems = cur;
		reader->ctxt->freeElemsNr++;
	    } else {
		xmlFree(cur);
	    }
	}
	cur = next;
    }
}

/**
 * xmlTextReaderFreeNode:
 * @reader:  the xmlTextReaderPtr used
 * @cur:  the node
 *
 * Free a node, this is a recursive behaviour, all the children are freed too.
 * This doesn't unlink the child from the list, use xmlUnlinkNode() first.
 */
static void
xmlTextReaderFreeNode(xmlTextReaderPtr reader, xmlNodePtr cur) {
    xmlDictPtr dict;

    dict = reader->ctxt->dict;
    if (cur->type == XML_DTD_NODE) {
	xmlFreeDtd((xmlDtdPtr) cur);
	return;
    }
    if (cur->type == XML_NAMESPACE_DECL) {
	xmlFreeNs((xmlNsPtr) cur);
        return;
    }
    if (cur->type == XML_ATTRIBUTE_NODE) {
	xmlTextReaderFreeProp(reader, (xmlAttrPtr) cur);
	return;
    }

    if ((cur->children != NULL) &&
	(cur->type != XML_ENTITY_REF_NODE)) {
	if (cur->children->parent == cur)
	    xmlTextReaderFreeNodeList(reader, cur->children);
	cur->children = NULL;
    }

    if ((__xmlRegisterCallbacks) && (xmlDeregisterNodeDefaultValue))
	xmlDeregisterNodeDefaultValue(cur);

    if (((cur->type == XML_ELEMENT_NODE) ||
	 (cur->type == XML_XINCLUDE_START) ||
	 (cur->type == XML_XINCLUDE_END)) &&
	(cur->properties != NULL))
	xmlTextReaderFreePropList(reader, cur->properties);
    if ((cur->type != XML_ELEMENT_NODE) &&
	(cur->type != XML_XINCLUDE_START) &&
	(cur->type != XML_XINCLUDE_END) &&
	(cur->type != XML_ENTITY_REF_NODE)) {
	DICT_FREE(cur->content);
    }
    if (((cur->type == XML_ELEMENT_NODE) ||
	 (cur->type == XML_XINCLUDE_START) ||
	 (cur->type == XML_XINCLUDE_END)) &&
	(cur->nsDef != NULL))
	xmlFreeNsList(cur->nsDef);

    /*
     * we don't free names here they are interned now
     */
    if ((cur->type != XML_TEXT_NODE) &&
        (cur->type != XML_COMMENT_NODE))
	DICT_FREE(cur->name);

    if (((cur->type == XML_ELEMENT_NODE) ||
	 (cur->type == XML_TEXT_NODE)) &&
	(reader != NULL) && (reader->ctxt != NULL) &&
	(reader->ctxt->freeElemsNr < 100)) {
	cur->next = reader->ctxt->freeElems;
	reader->ctxt->freeElems = cur;
	reader->ctxt->freeElemsNr++;
    } else {
	xmlFree(cur);
    }
}

/**
 * xmlTextReaderFreeIDTable:
 * @table:  An id table
 *
 * Deallocate the memory used by an ID hash table.
 */
static void
xmlTextReaderFreeIDTable(xmlIDTablePtr table) {
    xmlHashFree(table, (xmlHashDeallocator) xmlFreeID);
}

/**
 * xmlTextReaderFreeDoc:
 * @reader:  the xmlTextReaderPtr used
 * @cur:  pointer to the document
 *
 * Free up all the structures used by a document, tree included.
 */
static void
xmlTextReaderFreeDoc(xmlTextReaderPtr reader, xmlDocPtr cur) {
    xmlDtdPtr extSubset, intSubset;

    if (cur == NULL) return;

    if ((__xmlRegisterCallbacks) && (xmlDeregisterNodeDefaultValue))
	xmlDeregisterNodeDefaultValue((xmlNodePtr) cur);

    /*
     * Do this before freeing the children list to avoid ID lookups
     */
    if (cur->ids != NULL) xmlTextReaderFreeIDTable((xmlIDTablePtr) cur->ids);
    cur->ids = NULL;
    if (cur->refs != NULL) xmlFreeRefTable((xmlRefTablePtr) cur->refs);
    cur->refs = NULL;
    extSubset = cur->extSubset;
    intSubset = cur->intSubset;
    if (intSubset == extSubset)
	extSubset = NULL;
    if (extSubset != NULL) {
	xmlUnlinkNode((xmlNodePtr) cur->extSubset);
	cur->extSubset = NULL;
	xmlFreeDtd(extSubset);
    }
    if (intSubset != NULL) {
	xmlUnlinkNode((xmlNodePtr) cur->intSubset);
	cur->intSubset = NULL;
	xmlFreeDtd(intSubset);
    }

    if (cur->children != NULL) xmlTextReaderFreeNodeList(reader, cur->children);

    if (cur->version != NULL) xmlFree((char *) cur->version);
    if (cur->name != NULL) xmlFree((char *) cur->name);
    if (cur->encoding != NULL) xmlFree((char *) cur->encoding);
    if (cur->oldNs != NULL) xmlFreeNsList(cur->oldNs);
    if (cur->URL != NULL) xmlFree((char *) cur->URL);
    if (cur->dict != NULL) xmlDictFree(cur->dict);

    xmlFree(cur);
}

/************************************************************************
 *									*
 *			The reader core parser				*
 *									*
 ************************************************************************/
#ifdef DEBUG_READER
static void
xmlTextReaderDebug(xmlTextReaderPtr reader) {
    if ((reader == NULL) || (reader->ctxt == NULL)) {
	fprintf(stderr, "xmlTextReader NULL\n");
	return;
    }
    fprintf(stderr, "xmlTextReader: state %d depth %d ",
	    reader->state, reader->depth);
    if (reader->node == NULL) {
	fprintf(stderr, "node = NULL\n");
    } else {
	fprintf(stderr, "node %s\n", reader->node->name);
    }
    fprintf(stderr, "  input: base %d, cur %d, depth %d: ",
	    reader->base, reader->cur, reader->ctxt->nodeNr);
    if (reader->input->buffer == NULL) {
	fprintf(stderr, "buffer is NULL\n");
    } else {
#ifdef LIBXML_DEBUG_ENABLED
	xmlDebugDumpString(stderr,
		&reader->input->buffer->content[reader->cur]);
#endif
	fprintf(stderr, "\n");
    }
}
#endif

/**
 * xmlTextReaderEntPush:
 * @reader:  the xmlTextReaderPtr used
 * @value:  the entity reference node
 *
 * Pushes a new entity reference node on top of the entities stack
 *
 * Returns 0 in case of error, the index in the stack otherwise
 */
static int
xmlTextReaderEntPush(xmlTextReaderPtr reader, xmlNodePtr value)
{
    if (reader->entMax <= 0) {
	reader->entMax = 10;
	reader->entTab = (xmlNodePtr *) xmlMalloc(reader->entMax *
		                                  sizeof(reader->entTab[0]));
        if (reader->entTab == NULL) {
            xmlGenericError(xmlGenericErrorContext, "xmlMalloc failed !\n");
            return (0);
        }
    }
    if (reader->entNr >= reader->entMax) {
        reader->entMax *= 2;
        reader->entTab =
            (xmlNodePtr *) xmlRealloc(reader->entTab,
                                      reader->entMax *
                                      sizeof(reader->entTab[0]));
        if (reader->entTab == NULL) {
            xmlGenericError(xmlGenericErrorContext, "xmlRealloc failed !\n");
            return (0);
        }
    }
    reader->entTab[reader->entNr] = value;
    reader->ent = value;
    return (reader->entNr++);
}

/**
 * xmlTextReaderEntPop:
 * @reader:  the xmlTextReaderPtr used
 *
 * Pops the top element entity from the entities stack
 *
 * Returns the entity just removed
 */
static xmlNodePtr
xmlTextReaderEntPop(xmlTextReaderPtr reader)
{
    xmlNodePtr ret;

    if (reader->entNr <= 0)
        return (0);
    reader->entNr--;
    if (reader->entNr > 0)
        reader->ent = reader->entTab[reader->entNr - 1];
    else
        reader->ent = NULL;
    ret = reader->entTab[reader->entNr];
    reader->entTab[reader->entNr] = 0;
    return (ret);
}

/**
 * xmlTextReaderStartElement:
 * @ctx: the user data (XML parser context)
 * @fullname:  The element name, including namespace prefix
 * @atts:  An array of name/value attributes pairs, NULL terminated
 *
 * called when an opening tag has been processed.
 */
static void
xmlTextReaderStartElement(void *ctx, const xmlChar *fullname,
	                  const xmlChar **atts) {
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlTextReaderPtr reader = (xmlTextReaderPtr)ctxt->_private;

#ifdef DEBUG_CALLBACKS
    printf("xmlTextReaderStartElement(%s)\n", fullname);
#endif
    if ((reader != NULL) && (reader->startElement != NULL)) {
	reader->startElement(ctx, fullname, atts);
	if ((ctxt->node != NULL) && (ctxt->input != NULL) &&
	    (ctxt->input->cur != NULL) && (ctxt->input->cur[0] == '/') &&
	    (ctxt->input->cur[1] == '>'))
	    ctxt->node->extra = NODE_IS_EMPTY;
    }
    if (reader != NULL)
	reader->state = XML_TEXTREADER_ELEMENT;
}

/**
 * xmlTextReaderEndElement:
 * @ctx: the user data (XML parser context)
 * @fullname:  The element name, including namespace prefix
 *
 * called when an ending tag has been processed.
 */
static void
xmlTextReaderEndElement(void *ctx, const xmlChar *fullname) {
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlTextReaderPtr reader = (xmlTextReaderPtr)ctxt->_private;

#ifdef DEBUG_CALLBACKS
    printf("xmlTextReaderEndElement(%s)\n", fullname);
#endif
    if ((reader != NULL) && (reader->endElement != NULL)) {
	reader->endElement(ctx, fullname);
    }
}

/**
 * xmlTextReaderStartElementNs:
 * @ctx: the user data (XML parser context)
 * @localname:  the local name of the element
 * @prefix:  the element namespace prefix if available
 * @URI:  the element namespace name if available
 * @nb_namespaces:  number of namespace definitions on that node
 * @namespaces:  pointer to the array of prefix/URI pairs namespace definitions
 * @nb_attributes:  the number of attributes on that node
 * nb_defaulted:  the number of defaulted attributes.
 * @attributes:  pointer to the array of (localname/prefix/URI/value/end)
 *               attribute values.
 *
 * called when an opening tag has been processed.
 */
static void
xmlTextReaderStartElementNs(void *ctx,
                      const xmlChar *localname,
		      const xmlChar *prefix,
		      const xmlChar *URI,
		      int nb_namespaces,
		      const xmlChar **namespaces,
		      int nb_attributes,
		      int nb_defaulted,
		      const xmlChar **attributes)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlTextReaderPtr reader = (xmlTextReaderPtr)ctxt->_private;

#ifdef DEBUG_CALLBACKS
    printf("xmlTextReaderStartElementNs(%s)\n", localname);
#endif
    if ((reader != NULL) && (reader->startElementNs != NULL)) {
	reader->startElementNs(ctx, localname, prefix, URI, nb_namespaces,
	                       namespaces, nb_attributes, nb_defaulted,
			       attributes);
	if ((ctxt->node != NULL) && (ctxt->input != NULL) &&
	    (ctxt->input->cur != NULL) && (ctxt->input->cur[0] == '/') &&
	    (ctxt->input->cur[1] == '>'))
	    ctxt->node->extra = NODE_IS_EMPTY;
    }
    if (reader != NULL)
	reader->state = XML_TEXTREADER_ELEMENT;
}

/**
 * xmlTextReaderEndElementNs:
 * @ctx: the user data (XML parser context)
 * @localname:  the local name of the element
 * @prefix:  the element namespace prefix if available
 * @URI:  the element namespace name if available
 *
 * called when an ending tag has been processed.
 */
static void
xmlTextReaderEndElementNs(void *ctx,
                          const xmlChar * localname,
                          const xmlChar * prefix,
		          const xmlChar * URI)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlTextReaderPtr reader = (xmlTextReaderPtr)ctxt->_private;

#ifdef DEBUG_CALLBACKS
    printf("xmlTextReaderEndElementNs(%s)\n", localname);
#endif
    if ((reader != NULL) && (reader->endElementNs != NULL)) {
	reader->endElementNs(ctx, localname, prefix, URI);
    }
}


/**
 * xmlTextReaderCharacters:
 * @ctx: the user data (XML parser context)
 * @ch:  a xmlChar string
 * @len: the number of xmlChar
 *
 * receiving some chars from the parser.
 */
static void
xmlTextReaderCharacters(void *ctx, const xmlChar *ch, int len)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlTextReaderPtr reader = (xmlTextReaderPtr)ctxt->_private;

#ifdef DEBUG_CALLBACKS
    printf("xmlTextReaderCharacters()\n");
#endif
    if ((reader != NULL) && (reader->characters != NULL)) {
	reader->characters(ctx, ch, len);
    }
}

/**
 * xmlTextReaderCDataBlock:
 * @ctx: the user data (XML parser context)
 * @value:  The pcdata content
 * @len:  the block length
 *
 * called when a pcdata block has been parsed
 */
static void
xmlTextReaderCDataBlock(void *ctx, const xmlChar *ch, int len)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr) ctx;
    xmlTextReaderPtr reader = (xmlTextReaderPtr)ctxt->_private;

#ifdef DEBUG_CALLBACKS
    printf("xmlTextReaderCDataBlock()\n");
#endif
    if ((reader != NULL) && (reader->cdataBlock != NULL)) {
	reader->cdataBlock(ctx, ch, len);
    }
}

/**
 * xmlTextReaderPushData:
 * @reader:  the xmlTextReaderPtr used
 *
 * Push data down the progressive parser until a significant callback
 * got raised.
 *
 * Returns -1 in case of failure, 0 otherwise
 */
static int
xmlTextReaderPushData(xmlTextReaderPtr reader) {
    xmlBufferPtr inbuf;
    int val, s;
    xmlTextReaderState oldstate;

    if ((reader->input == NULL) || (reader->input->buffer == NULL))
	return(-1);

    oldstate = reader->state;
    reader->state = XML_TEXTREADER_NONE;
    inbuf = reader->input->buffer;

    while (reader->state == XML_TEXTREADER_NONE) {
	if (inbuf->use < reader->cur + CHUNK_SIZE) {
	    /*
	     * Refill the buffer unless we are at the end of the stream
	     */
	    if (reader->mode != XML_TEXTREADER_MODE_EOF) {
		val = xmlParserInputBufferRead(reader->input, 4096);
		if ((val == 0) &&
		    (inbuf->alloc == XML_BUFFER_ALLOC_IMMUTABLE)) {
		    if (inbuf->use == reader->cur) {
			reader->mode = XML_TEXTREADER_MODE_EOF;
			reader->state = oldstate;
		    }
		} else if (val < 0) {
		    reader->mode = XML_TEXTREADER_MODE_EOF;
		    reader->state = oldstate;
		    if ((oldstate != XML_TEXTREADER_START) ||
			(reader->ctxt->myDoc != NULL))
			return(val);
		} else if (val == 0) {
		    /* mark the end of the stream and process the remains */
		    reader->mode = XML_TEXTREADER_MODE_EOF;
		    break;
		}

	    } else 
		break;
	}
	/*
	 * parse by block of CHUNK_SIZE bytes, various tests show that
	 * it's the best tradeoff at least on a 1.2GH Duron
	 */
	if (inbuf->use >= reader->cur + CHUNK_SIZE) {
	    val = xmlParseChunk(reader->ctxt,
		          (const char *) &inbuf->content[reader->cur],
			  CHUNK_SIZE, 0);
	    reader->cur += CHUNK_SIZE;
	    if ((val != 0) || (reader->ctxt->wellFormed == 0))
		return(-1);
	} else {
	    s = inbuf->use - reader->cur;
	    val = xmlParseChunk(reader->ctxt,
		          (const char *) &inbuf->content[reader->cur],
			  s, 0);
	    reader->cur += s;
	    if ((val != 0) || (reader->ctxt->wellFormed == 0))
		return(-1);
	    break;
	}
    }

    /*
     * Discard the consumed input when needed and possible
     */
    if (reader->mode == XML_TEXTREADER_MODE_INTERACTIVE) {
        if (inbuf->alloc != XML_BUFFER_ALLOC_IMMUTABLE) {
	    if ((reader->cur >= 4096) &&
		(inbuf->use - reader->cur <= CHUNK_SIZE)) {
		val = xmlBufferShrink(inbuf, reader->cur);
		if (val >= 0) {
		    reader->cur -= val;
		}
	    }
	}
    }

    /*
     * At the end of the stream signal that the work is done to the Push
     * parser.
     */
    else if (reader->mode == XML_TEXTREADER_MODE_EOF) {
	if (reader->mode != XML_TEXTREADER_DONE) {
	    s = inbuf->use - reader->cur;
	    val = xmlParseChunk(reader->ctxt,
		    (const char *) &inbuf->content[reader->cur], 
		    s, 1);
	    reader->cur = inbuf->use;
	    reader->mode = XML_TEXTREADER_DONE;
	    if ((val != 0) || (reader->ctxt->wellFormed == 0))
	        return(-1);
	}
    }
    reader->state = oldstate;
    return(0);
}

#ifdef LIBXML_REGEXP_ENABLED
/**
 * xmlTextReaderValidatePush:
 * @reader:  the xmlTextReaderPtr used
 *
 * Push the current node for validation
 */
static void
xmlTextReaderValidatePush(xmlTextReaderPtr reader ATTRIBUTE_UNUSED) {
    xmlNodePtr node = reader->node;

#ifdef LIBXML_VALID_ENABLED
    if ((reader->validate == XML_TEXTREADER_VALIDATE_DTD) &&
        (reader->ctxt != NULL) && (reader->ctxt->validate == 1)) {
	if ((node->ns == NULL) || (node->ns->prefix == NULL)) {
	    reader->ctxt->valid &= xmlValidatePushElement(&reader->ctxt->vctxt,
				    reader->ctxt->myDoc, node, node->name);
	} else {
	    /* TODO use the BuildQName interface */
	    xmlChar *qname;

	    qname = xmlStrdup(node->ns->prefix);
	    qname = xmlStrcat(qname, BAD_CAST ":");
	    qname = xmlStrcat(qname, node->name);
	    reader->ctxt->valid &= xmlValidatePushElement(&reader->ctxt->vctxt,
				    reader->ctxt->myDoc, node, qname);
	    if (qname != NULL)
		xmlFree(qname);
	}
    }
#endif /* LIBXML_VALID_ENABLED */
#ifdef LIBXML_SCHEMAS_ENABLED
    if ((reader->validate == XML_TEXTREADER_VALIDATE_RNG) &&
               (reader->rngValidCtxt != NULL)) {
	int ret;

	if (reader->rngFullNode != NULL) return;
	ret = xmlRelaxNGValidatePushElement(reader->rngValidCtxt,
	                                    reader->ctxt->myDoc,
					    node);
	if (ret == 0) {
	    /*
	     * this element requires a full tree
	     */
	    node = xmlTextReaderExpand(reader);
	    if (node == NULL) {
printf("Expand failed !\n");
	        ret = -1;
	    } else {
		ret = xmlRelaxNGValidateFullElement(reader->rngValidCtxt,
						    reader->ctxt->myDoc,
						    node);
		reader->rngFullNode = node;
	    }
	}
	if (ret != 1)
	    reader->rngValidErrors++;
    }
#endif
}

/**
 * xmlTextReaderValidateCData:
 * @reader:  the xmlTextReaderPtr used
 * @data:  pointer to the CData
 * @len:  lenght of the CData block in bytes.
 *
 * Push some CData for validation
 */
static void
xmlTextReaderValidateCData(xmlTextReaderPtr reader,
                           const xmlChar *data, int len) {
#ifdef LIBXML_VALID_ENABLED
    if ((reader->validate == XML_TEXTREADER_VALIDATE_DTD) &&
        (reader->ctxt != NULL) && (reader->ctxt->validate == 1)) {
	reader->ctxt->valid &= xmlValidatePushCData(&reader->ctxt->vctxt,
	                                            data, len);
    }
#endif /* LIBXML_VALID_ENABLED */
#ifdef LIBXML_SCHEMAS_ENABLED
    if ((reader->validate == XML_TEXTREADER_VALIDATE_RNG) &&
               (reader->rngValidCtxt != NULL)) {
	int ret;

	if (reader->rngFullNode != NULL) return;
	ret = xmlRelaxNGValidatePushCData(reader->rngValidCtxt, data, len);
	if (ret != 1)
	    reader->rngValidErrors++;
    }
#endif
}

/**
 * xmlTextReaderValidatePop:
 * @reader:  the xmlTextReaderPtr used
 *
 * Pop the current node from validation
 */
static void
xmlTextReaderValidatePop(xmlTextReaderPtr reader) {
    xmlNodePtr node = reader->node;

#ifdef LIBXML_VALID_ENABLED
    if ((reader->validate == XML_TEXTREADER_VALIDATE_DTD) &&
        (reader->ctxt != NULL) && (reader->ctxt->validate == 1)) {
	if ((node->ns == NULL) || (node->ns->prefix == NULL)) {
	    reader->ctxt->valid &= xmlValidatePopElement(&reader->ctxt->vctxt,
				    reader->ctxt->myDoc, node, node->name);
	} else {
	    /* TODO use the BuildQName interface */
	    xmlChar *qname;

	    qname = xmlStrdup(node->ns->prefix);
	    qname = xmlStrcat(qname, BAD_CAST ":");
	    qname = xmlStrcat(qname, node->name);
	    reader->ctxt->valid &= xmlValidatePopElement(&reader->ctxt->vctxt,
				    reader->ctxt->myDoc, node, qname);
	    if (qname != NULL)
		xmlFree(qname);
	}
    }
#endif /* LIBXML_VALID_ENABLED */
#ifdef LIBXML_SCHEMAS_ENABLED
    if ((reader->validate == XML_TEXTREADER_VALIDATE_RNG) &&
               (reader->rngValidCtxt != NULL)) {
	int ret;

	if (reader->rngFullNode != NULL) {
	    if (node == reader->rngFullNode) 
	        reader->rngFullNode = NULL;
	    return;
	}
	ret = xmlRelaxNGValidatePopElement(reader->rngValidCtxt,
	                                   reader->ctxt->myDoc,
					   node);
	if (ret != 1)
	    reader->rngValidErrors++;
    }
#endif
}

/**
 * xmlTextReaderValidateEntity:
 * @reader:  the xmlTextReaderPtr used
 *
 * Handle the validation when an entity reference is encountered and
 * entity substitution is not activated. As a result the parser interface
 * must walk through the entity and do the validation calls
 */
static void
xmlTextReaderValidateEntity(xmlTextReaderPtr reader) {
    xmlNodePtr oldnode = reader->node;
    xmlNodePtr node = reader->node;
    xmlParserCtxtPtr ctxt = reader->ctxt;

    do {
	if (node->type == XML_ENTITY_REF_NODE) {
	    /*
	     * Case where the underlying tree is not availble, lookup the entity
	     * and walk it.
	     */
	    if ((node->children == NULL) && (ctxt->sax != NULL) &&
		(ctxt->sax->getEntity != NULL)) {
		node->children = (xmlNodePtr) 
		    ctxt->sax->getEntity(ctxt, node->name);
	    }

	    if ((node->children != NULL) &&
		(node->children->type == XML_ENTITY_DECL) &&
		(node->children->children != NULL)) {
		xmlTextReaderEntPush(reader, node);
		node = node->children->children;
		continue;
	    } else {
		/*
		 * The error has probably be raised already.
		 */
		if (node == oldnode)
		    break;
		node = node->next;
	    }
#ifdef LIBXML_REGEXP_ENABLED
	} else if (node->type == XML_ELEMENT_NODE) {
	    reader->node = node;
	    xmlTextReaderValidatePush(reader);
	} else if ((node->type == XML_TEXT_NODE) ||
		   (node->type == XML_CDATA_SECTION_NODE)) {
            xmlTextReaderValidateCData(reader, node->content,
	                               xmlStrlen(node->content));
#endif
	}

	/*
	 * go to next node
	 */
	if (node->children != NULL) {
	    node = node->children;
	    continue;
	} else if (node->type == XML_ELEMENT_NODE) {
	    xmlTextReaderValidatePop(reader);
	}
	if (node->next != NULL) {
	    node = node->next;
	    continue;
	}
	do {
	    node = node->parent;
	    if (node->type == XML_ELEMENT_NODE) {
	        xmlNodePtr tmp;
		if (reader->entNr == 0) {
		    while ((tmp = node->last) != NULL) {
			if ((tmp->extra & NODE_IS_PRESERVED) == 0) {
			    xmlUnlinkNode(tmp);
			    xmlTextReaderFreeNode(reader, tmp);
			} else
			    break;
		    }
		}
		reader->node = node;
		xmlTextReaderValidatePop(reader);
	    }
	    if ((node->type == XML_ENTITY_DECL) &&
		(reader->ent != NULL) && (reader->ent->children == node)) {
		node = xmlTextReaderEntPop(reader);
	    }
	    if (node == oldnode)
		break;
	    if (node->next != NULL) {
		node = node->next;
		break;
	    }
	} while ((node != NULL) && (node != oldnode));
    } while ((node != NULL) && (node != oldnode));
    reader->node = oldnode;
}
#endif /* LIBXML_REGEXP_ENABLED */


/**
 * xmlTextReaderGetSuccessor:
 * @cur:  the current node
 *
 * Get the successor of a node if available.
 *
 * Returns the successor node or NULL
 */
static xmlNodePtr
xmlTextReaderGetSuccessor(xmlNodePtr cur) {
    if (cur == NULL) return(NULL) ; /* ERROR */
    if (cur->next != NULL) return(cur->next) ;
    do {
        cur = cur->parent;
        if (cur == NULL) return(NULL);
        if (cur->next != NULL) return(cur->next);
    } while (cur != NULL);
    return(cur);
}

/**
 * xmlTextReaderDoExpand:
 * @reader:  the xmlTextReaderPtr used
 *
 * Makes sure that the current node is fully read as wel