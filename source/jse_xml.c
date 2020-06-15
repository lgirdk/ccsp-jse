
#include <libxml/parser.h>
#include "jse_debug.h"
#include "jse_xml.h"

static void iterate_array(duk_context * ctx, duk_idx_t obj_idx, xmlNodePtr parent, const char* name);
static void walk_object(duk_context * ctx, duk_idx_t obj_idx, xmlNodePtr parent);

/** Reference count for binding */
static int ref_count = 0;

/**
 * Generates the XML for a child, handling an array or object appropriately
 *
 * This function generates the XML for a child on the bottom of the stack.
 * If the child is an array it will iterate the array. If the child is an
 * object, it will walk the object. Otherwise it will set the value of the
 * node to be the value of the child.
 *
 * @param ctx the duktape context.
 * @param parent the parent node.
 * @param name the name to use for the child node.
 */
static void child_to_xml(duk_context * ctx, xmlNodePtr parent, const char* name)
{
    if (duk_is_array(ctx, -1))
    {
        iterate_array(ctx, -1, parent, duk_safe_to_string(ctx, -2));
    }
    else
    {
        xmlNodePtr child = xmlNewChild(
            parent, NULL, BAD_CAST name, NULL);
        if (child != NULL)
        {
            if (duk_is_object(ctx, -1))
            {
                /* Recurse in to object */
                walk_object(ctx, -1, child);
            }
            else
            {
                xmlNodeSetContent(child, BAD_CAST duk_safe_to_string(ctx, -1));
            }
        }
    }
}

/**
 * Iterates over an array added nodes for each element.
 *
 * @param ctx the duktape context.
 * @param obj_idx the index of the array to iterate.
 * @param parent the parent node.
 * @param name the name to use for the child nodes.
 */
static void iterate_array(duk_context * ctx, duk_idx_t obj_idx, xmlNodePtr parent, const char* name)
{
    duk_size_t len = 0;
    duk_size_t idx = 0;

    len = duk_get_length(ctx, obj_idx);
    for (idx = 0; idx < len; idx ++)
    {
        duk_get_prop_index(ctx, obj_idx, idx);
        child_to_xml(ctx, parent, name);
        duk_pop(ctx);
    }
}

/**
 * Recursively walk over a JavaScript object making an XML tree.
 *
 * @param ctx the duktape context.
 * @param obj_idx the index of the object to walk.
 * @param parent the parent node.
 */
static void walk_object(duk_context * ctx, duk_idx_t obj_idx, xmlNodePtr parent)
{
    /* Pushes an enumerator on to the stack */
    duk_enum(ctx, obj_idx, 0);

    /* Get the value. After duk_next() the stack has the enumerator at -3,
       the key at -2 and the value at -1 */
    while (duk_next(ctx, obj_idx, true))
    {
        child_to_xml(ctx, parent, duk_safe_to_string(ctx, -2));

        /* Pop the key and the value */
        duk_pop_2(ctx);
    }

    /* Pop the enumerator */
    duk_pop(ctx);
}


/**
 * Runs JavaScript code stored in a file, the name of which is on the stack.
 *
 * @param jse_ctx the jse context.
 * @param buffer the buffer.
 * @param size the size of the context.
 * @return an error status or 0.
 */
static duk_ret_t do_objectToXMLString(duk_context * ctx)
{
    duk_ret_t ret = DUK_RET_ERROR;

    if (duk_is_string(ctx, -2))
    {
        xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
        if (doc != NULL)
        {
            xmlNodePtr root = xmlNewNode(NULL, BAD_CAST duk_safe_to_string(ctx, -2));
            xmlChar *xml_buffer;
            int xml_size;

            if (root != NULL)
            {
                /* You can't do arrays of the root node. To include an array create a JS
                   object eg: { name: [ val1, val2, val3 ] } */
                if (duk_is_object(ctx, -1))
                {
                    walk_object(ctx, -1, root);
                }
                else
                {
                    JSE_WARNING("\"object\" is not an object: %d", duk_get_type(ctx, -1))

                    /*  Convert what ever it really is to a string */
                    xmlNodeSetContent(root, BAD_CAST duk_safe_to_string(ctx, -1));
                }

                xmlDocSetRootElement(doc, root);
            }

            xmlDocDumpFormatMemory(doc, &xml_buffer, &xml_size, 1);
            duk_push_lstring(ctx, (char*)xml_buffer, (duk_size_t)xml_size);

            xmlFree(xml_buffer);
            xmlFreeDoc(doc);
            ret = 1;
        }
        else
        {
            JSE_ERROR("Failed to create XML doc")
        }

    }
    else
    {
        JSE_ERROR("Invalid argument \"name\" (%d)", duk_get_type(ctx, -2))
        ret = DUK_RET_TYPE_ERROR;
    }
    
    JSE_VERBOSE("ret=%d", ret)
    return ret;
}

/**
 * Binds a set of JavaScript relating to XML manipulation
 *
 * @param jse_ctx the jse context.
 * @return an error status or 0.
 */
duk_int_t jse_bind_xml(jse_context_t* jse_ctx)
{
    duk_int_t ret = DUK_ERR_ERROR;

    JSE_VERBOSE("Binding XML functions!")

    JSE_VERBOSE("ref_count=%d", ref_count)
    if (jse_ctx != NULL)
    {
        if (ref_count == 0)
        {
            duk_push_c_function(jse_ctx->ctx, do_objectToXMLString, 2);
            duk_put_global_string(jse_ctx->ctx, "objectToXMLString");
        }

        ref_count ++;
        ret = 0;
    }

    return ret;
}

/**
 * Unbinds the JavaScript extensions.
 *
 * Actually just decrements the reference count. Needed for fast cgi
 * since the same process will rebind. Not unbinding is not an issue
 * as the duktape context is destroyed each time cleaning everything
 * up.
 *
 * @param jse_ctx the jse context.
 */
void jse_unbind_xml(jse_context_t * jse_ctx)
{
    /* Stop unused warning */
    jse_ctx = jse_ctx;

    ref_count --;
    JSE_VERBOSE("ref_count=%d", ref_count)

    /* TODO: Actually unbind */
}
