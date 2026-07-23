#include "tree_sitter/alloc.h"
#include "tree_sitter/array.h"
#include "tree_sitter/parser.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum TokenType
{
    TEXT_BLOCK_START,
    TEXT_BLOCK_BLANK_LINE,
    TEXT_BLOCK_INDENT,
    TEXT_BLOCK_LINE_CONTENT,
    TEXT_BLOCK_END,
} TokenType;

typedef Array(char) CharArray;

inline static void consume(TSLexer *lexer)
{
    lexer->advance(lexer, false);
}

/**
 * Skips or consumes the next character if it matches the given one.
 *
 * \return `true` if the character matched, `false` otherwise.
 */
inline static bool match(TSLexer *lexer, const char ch, bool skip)
{
    if (lexer->lookahead != ch)
        return false;

    lexer->advance(lexer, skip);
    return true;
}

inline static bool consume_if_match(TSLexer *lexer, const char ch)
{
    return match(lexer, ch, false);
}

/**
 * Consumes the next characters if they match the given string.
 *
 * \return `true` if every character matched, `false` on a mismatch or EOF.
 */
inline static bool consume_all(TSLexer *lexer, char const *string)
{
    for (char const *p = string; *p != '\0'; ++p)
        if (lexer->eof(lexer) || !consume_if_match(lexer, *p))
            return false;

    return true;
}

/**
 * Consumes the input until the cursor is past the first occurrence of the given character.
 *
 * \return `false` if the cursor reached the EOF first, otherwise `true`.
 */
inline static bool consume_past(TSLexer *lexer, const char ch)
{
    while (true)
        if (lexer->eof(lexer))
            return false;
        else if (lexer->lookahead != ch)
            consume(lexer);
        else
        {
            consume(lexer);
            return true;
        }
}

/**
 * Skips or consumes the next character if it matches any character in the given string.
 *
 * \return `true` if a character matched, `false` on no match or EOF.
 */
inline static bool match_any(TSLexer *lexer, char const *charset, bool skip)
{
    for (char const *p = charset; !lexer->eof(lexer) && *p != '\0'; ++p)
        if (match(lexer, *p, skip))
            return true;

    return false;
}

/**
 * Skips or consumes the input repeatedly when the next character is in the given character set.
 *
 * \return `true` if the cursor stopped on more input, `false` if it reached EOF.
 */
inline static bool advance_while_any(TSLexer *lexer, char const *charset, bool skip)
{
    while (match_any(lexer, charset, skip))
        ;

    return !lexer->eof(lexer);
}

inline static bool consume_while_any(TSLexer *lexer, char const *charset)
{
    return advance_while_any(lexer, charset, false);
}

inline static bool skip_while_any(TSLexer *lexer, char const *charset)
{
    return advance_while_any(lexer, charset, true);
}

inline static bool emit_token(TSLexer *lexer, TokenType type)
{
    lexer->mark_end(lexer);
    lexer->result_symbol = type;
    return true;
}

void *tree_sitter_jsonnet_external_scanner_create()
{
    // An array storing the initial indentation of the text block.
    return ts_calloc(1, sizeof(CharArray));
}

void tree_sitter_jsonnet_external_scanner_destroy(void *payload)
{
    CharArray *indent = (CharArray *)payload;
    array_delete(indent);
    ts_free(payload);
}

unsigned tree_sitter_jsonnet_external_scanner_serialize(void *payload, char *buffer)
{
    CharArray *indent = (CharArray *)payload;

    uint32_t size = indent->size;
    memcpy(buffer, &size, sizeof(size));

    for (uint32_t i = 0; i < indent->size; ++i)
        buffer[sizeof(size) + i] = *array_get(indent, i);

    return sizeof(size) + indent->size;
}

void tree_sitter_jsonnet_external_scanner_deserialize(void *payload, const char *buffer, unsigned length)
{
    CharArray *indent = (CharArray *)payload;
    array_clear(indent);

    if (length == 0)
        return;

    uint32_t size;
    memcpy(&size, buffer, sizeof(size));

    for (uint32_t i = 0; i < size; ++i)
        array_push(indent, buffer[sizeof(size) + i]);
}

inline static bool scan_text_block_start(void *payload, TSLexer *lexer)
{
    (void)payload;

    // Skips whitespaces before the starting fence.
    //
    // NOTE: Not consuming comments here is safe. Comments start with a slash, but this scanner never emits a token
    // starting with a slash, so on a comment it returns `false` and lets the internal lexer scan the comment.
    skip_while_any(lexer, " \t\n");

    if (!consume_all(lexer, "|||"))
        return false;

    // Scans the optional trailing dash, indicating that the last newline of the text block must be removed.
    consume_if_match(lexer, '-');

    // The starting fence must end with zero or more whitespaces and a newline.
    if (!consume_while_any(lexer, " \t") || !consume_if_match(lexer, '\n'))
        return false;

    return emit_token(lexer, TEXT_BLOCK_START);
}

inline static bool scan_text_block_end(void *payload, TSLexer *lexer)
{
    CharArray *indent = (CharArray *)payload;

    if (!consume_all(lexer, "|||"))
        return false;

    array_clear(indent);
    return emit_token(lexer, TEXT_BLOCK_END);
}

inline static bool scan_text_block_blank_line(void *payload, TSLexer *lexer)
{
    (void)payload;

    // Scans a newline-only blank line.
    if (!consume_if_match(lexer, '\n'))
        return false;

    return emit_token(lexer, TEXT_BLOCK_BLANK_LINE);
}

inline static bool scan_text_block_initial_indent(void *payload, TSLexer *lexer)
{
    CharArray *indent = (CharArray *)payload;

    // Scans the initial indentation.
    while (!lexer->eof(lexer) && (lexer->lookahead == ' ' || lexer->lookahead == '\t'))
    {
        if (indent->size >= TREE_SITTER_SERIALIZATION_BUFFER_SIZE - sizeof(indent->size))
            // The indentation is too long to fit in the 1024-byte scanner state buffer.
            return false;

        array_push(indent, (char)lexer->lookahead);
        consume(lexer);
    }

    if (indent->size == 0)
        // Empty indentation
        return false;

    return emit_token(lexer, TEXT_BLOCK_INDENT);
}

inline static bool scan_text_block_subsequent_indent(void *payload, TSLexer *lexer)
{
    CharArray *indent = (CharArray *)payload;

    for (uint32_t i = 0; i < indent->size; ++i)
        if (!consume_if_match(lexer, *array_get(indent, i)))
            return false;

    return emit_token(lexer, TEXT_BLOCK_INDENT);
}

inline static bool scan_text_block_indent(void *payload, TSLexer *lexer)
{
    CharArray *indent = (CharArray *)payload;
    return indent->size == 0 ? scan_text_block_initial_indent(payload, lexer)
                             : scan_text_block_subsequent_indent(payload, lexer);
}

inline static bool scan_text_block_line_content(void *payload, TSLexer *lexer)
{
    (void)payload;

    // Consumes through the newline that terminates the line content.
    if (!consume_past(lexer, '\n'))
        // Reached the EOF prematurely before seeing the newline.
        return false;

    return emit_token(lexer, TEXT_BLOCK_LINE_CONTENT);
}

bool tree_sitter_jsonnet_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols)
{
    return (valid_symbols[TEXT_BLOCK_START] && scan_text_block_start(payload, lexer)) ||
           (valid_symbols[TEXT_BLOCK_BLANK_LINE] && scan_text_block_blank_line(payload, lexer)) ||
           (valid_symbols[TEXT_BLOCK_INDENT] && scan_text_block_indent(payload, lexer)) ||
           (valid_symbols[TEXT_BLOCK_LINE_CONTENT] && scan_text_block_line_content(payload, lexer)) ||
           (valid_symbols[TEXT_BLOCK_END] && scan_text_block_end(payload, lexer));
}
