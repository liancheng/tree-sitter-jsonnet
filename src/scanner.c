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

/**
 * Tries to match a single character.
 *
 * When the next character matches the given one, returns `true` and advances the cursor by one character. Otherwise,
 * returns `false`.
 */
inline static bool match(TSLexer *lexer, const char ch)
{
    if (lexer->lookahead != ch)
        return false;

    lexer->advance(lexer, false);
    return true;
}

/**
 * Tries to match a given string.
 *
 * When the subsequent input matches the given string pattern, returns `true` and advances the cursor by the length of
 * the pattern. Otherwise, returns `false`.
 */
inline static bool match_all(TSLexer *lexer, char const *string)
{
    for (char const *p = string; *p != '\0'; ++p)
        if (lexer->eof(lexer) || !match(lexer, *p))
            return false;

    return true;
}

/**
 * Tries to match any character in the given string.
 *
 * When the next character matches any character in the given string, returns `true` and advances the cursor by one
 * character. Otherwise, returns `false`.
 */
inline static bool match_any(TSLexer *lexer, char const *charset)
{
    for (char const *p = charset; !lexer->eof(lexer) && *p != '\0'; ++p)
        if (match(lexer, *p))
            return true;

    return false;
}

/**
 * Advances the cursor after the first occurrence of the given character.
 *
 * If the cursor indeed advanced after the first occurrence of the given character, returns `true`. Otherwise, returns
 * `false`, indicating the cursor has reached the EOF.
 */
inline static bool advance_after(TSLexer *lexer, const char ch)
{
    while (true)
    {
        if (lexer->eof(lexer))
            return false;

        if (lexer->lookahead == ch)
        {
            lexer->advance(lexer, false);
            return true;
        }

        lexer->advance(lexer, false);
    }
}

/**
 * Advances the cursor repeatedly when the next character is in the given character set.
 *
 * If the cursor indeed reached a character not in the character set, returns `true`. Otherwise, returns `false`,
 * indicating that the cursor has reached the EOF.
 */
inline static bool advance_while_any(TSLexer *lexer, char const *charset)
{
    while (match_any(lexer, charset))
        ;

    return !lexer->eof(lexer);
}

inline static bool match_indent(TSLexer *lexer, CharArray *indent_chars)
{
    for (uint32_t i = 0; i < indent_chars->size; ++i)
        if (!match(lexer, *array_get(indent_chars, i)))
            return false;

    return true;
}

inline static bool scan_text_block_start(void *payload, TSLexer *lexer)
{
    (void)payload;

    if (!match_all(lexer, "|||"))
        return false;

    // Scans the optional trailing dash, indicating that the last newline of the text block must be removed.
    match(lexer, '-');

    // The starting fence must end with zero or more whitespaces and a newline.
    if (!advance_while_any(lexer, " \t") || !match(lexer, '\n'))
        return false;

    lexer->mark_end(lexer);
    lexer->result_symbol = TEXT_BLOCK_START;
    return true;
}

inline static bool scan_text_block_end(void *payload, TSLexer *lexer)
{
    CharArray *indent = (CharArray *)payload;

    if (!match_all(lexer, "|||"))
        return false;

    array_clear(indent);
    lexer->mark_end(lexer);
    lexer->result_symbol = TEXT_BLOCK_END;
    return true;
}

inline static bool scan_text_block_blank_line(void *payload, TSLexer *lexer)
{
    (void)payload;

    // Scans a newline-only blank line.
    if (!match(lexer, '\n'))
        return false;

    lexer->mark_end(lexer);
    lexer->result_symbol = TEXT_BLOCK_BLANK_LINE;
    return true;
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
        lexer->advance(lexer, false);
    }

    if (indent->size == 0)
        // Empty indentation
        return false;

    lexer->mark_end(lexer);
    lexer->result_symbol = TEXT_BLOCK_INDENT;
    return true;
}

inline static bool scan_text_block_subsequent_indent(void *payload, TSLexer *lexer)
{
    CharArray *indent = (CharArray *)payload;

    if (!match_indent(lexer, indent))
        return false;

    lexer->mark_end(lexer);
    lexer->result_symbol = TEXT_BLOCK_INDENT;
    return true;
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

    if (!advance_after(lexer, '\n'))
        // Reached the EOF prematurely before seeing the newline.
        return false;

    lexer->mark_end(lexer);
    lexer->result_symbol = TEXT_BLOCK_LINE_CONTENT;
    return true;
}

bool tree_sitter_jsonnet_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols)
{
    return (valid_symbols[TEXT_BLOCK_START] && scan_text_block_start(payload, lexer)) ||
           (valid_symbols[TEXT_BLOCK_BLANK_LINE] && scan_text_block_blank_line(payload, lexer)) ||
           (valid_symbols[TEXT_BLOCK_INDENT] && scan_text_block_indent(payload, lexer)) ||
           (valid_symbols[TEXT_BLOCK_LINE_CONTENT] && scan_text_block_line_content(payload, lexer)) ||
           (valid_symbols[TEXT_BLOCK_END] && scan_text_block_end(payload, lexer));
}

// vim:tw=120
