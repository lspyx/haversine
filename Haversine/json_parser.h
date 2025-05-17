#pragma once
#include "common.h"
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
struct buffer {
    size_t count;
    uint8_t* data;
};

#define CONSTANT_STRING(string) \
    buffer { sizeof(string) - 1, (uint8_t*)(string) }

bool is_in_bounds(buffer source, uint64_t at)
{
    bool result = (at < source.count);
    return result;
}

bool are_equal(buffer a, buffer b)
{
    if (a.count != b.count) {
        return false;
    }

    for (uint64_t i = 0; i < a.count; ++i) {
        if (a.data[i] != b.data[i]) {
            return false;
        }
    }

    return true;
}

buffer allocate_buffer(size_t count)
{
    buffer res = {};
    res.data = (uint8_t*)malloc(count);
    if (res.data) {
        res.count = count;
    } else {
        fprintf(stderr, "ERROR: Unable to allocate %llu bytes.\n", count);
    }

    return res;
}

void free_buffer(buffer* buffer)
{
    if (buffer->data) {
        free(buffer->data);
    }
    *buffer = {};
}

namespace json {
enum class eJsonTokenType {
    EndOfStream,
    Error,
    OpenBrace,
    OpenBracket,
    CloseBrace,
    CloseBracket,
    Comma,
    Colon,
    SemiColon,
    StringLiteral,
    Number,
    True,
    False,
    Null,
    Count
};

struct json_token {
public:
    eJsonTokenType type;
    buffer value;
};

struct json_element {
    buffer label;
    buffer value;
    json_element* first_sub_element;
    json_element* next_sibling;
};

struct json_parser {
    buffer source;
    uint64_t at;
    bool had_error;
};

bool is_json_digit(buffer source, uint64_t at)
{
    bool res = false;
    if (is_in_bounds(source, at)) {
        uint8_t value = source.data[at];
        res = ((value >= '0') && (value <= '9'));
    }
    return res;
}

bool is_json_whitespace(buffer source, uint64_t at)
{
    bool res = false;
    if (is_in_bounds(source, at)) {
        uint8_t value = source.data[at];
        res = ((value == ' ') || (value == '\t') || (value == '\n') || (value == '\r'));
    }
    return res;
}

bool is_parsing(json_parser* parser)
{
    bool result = !parser->had_error && is_in_bounds(parser->source, parser->at);
    return result;
}

void error(json_parser* parser, json_token token, char const* message)
{
    parser->had_error = true;
    fprintf(stderr, "Error: \"%.*s\" - %s\n",
        static_cast<uint32_t>(token.value.count),
        reinterpret_cast<char*>(token.value.data), message);
}

void parse_keyword(buffer source, uint64_t* at, buffer keyword_remaining,
    eJsonTokenType type, json_token* result)
{
    if (source.count - *at >= keyword_remaining.count) {
        buffer check = source;
        check.data += *at;
        check.count = keyword_remaining.count;
        if (are_equal(check, keyword_remaining)) {
            result->type = type;
            result->value.count += keyword_remaining.count;
            *at += keyword_remaining.count;
        }
    }
}

json_token get_json_token(json_parser* parser)
{
    json_token result = {};
    buffer source = parser->source;
    uint64_t at = parser->at;

    while (is_json_whitespace(source, at)) {
        ++at;
    }

    if (is_in_bounds(source, at)) {
        result.type = eJsonTokenType::Error;
        result.value.count = 1;
        result.value.data = source.data + at;
        uint8_t val = source.data[at++];
        switch (val) {
        case '{':
            result.type = eJsonTokenType::OpenBrace;
            break;
        case '[':
            result.type = eJsonTokenType::OpenBracket;
            break;
        case '}':
            result.type = eJsonTokenType::CloseBrace;
            break;
        case ']':
            result.type = eJsonTokenType::CloseBracket;
            break;
        case ',':
            result.type = eJsonTokenType::Comma;
            break;
        case ':':
            result.type = eJsonTokenType::Colon;
            break;
        case ';':
            result.type = eJsonTokenType::SemiColon;
            break;

        case 'f': {
            parse_keyword(source, &at, CONSTANT_STRING("alse"), eJsonTokenType::False,
                &result);
        } break;
        case 'n': {
            parse_keyword(source, &at, CONSTANT_STRING("ull"), eJsonTokenType::Null,
                &result);
        } break;
        case 't': {
            parse_keyword(source, &at, CONSTANT_STRING("rue"), eJsonTokenType::True,
                &result);
        } break;
        case '"': {
            result.type = eJsonTokenType::StringLiteral;
            uint64_t string_start = at;
            while (is_in_bounds(source, at) && (source.data[at] != '"')) {
                if (is_in_bounds(source, at + 1) && (source.data[at] == '\\') && (source.data[at + 1] == '"')) {
                    ++at;
                }
                ++at;
            }
            result.value.data = source.data + string_start;
            result.value.count = at - string_start;
            if (is_in_bounds(source, at))
                ++at;
        } break;
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
            uint64_t start = at - 1;
            result.type = eJsonTokenType::Number;
            if (val == '-' && is_in_bounds(source, at))
                val = source.data[at++];
            if (val != '0') {
                while (is_json_digit(source, at)) {
                    ++at;
                }
            }
            if (is_in_bounds(source, at) && (source.data[at] == '.')) {
                ++at;
                while (is_json_digit(source, at)) {
                    ++at;
                }
            }
            if (is_in_bounds(source, at)
                && ((source.data[at] == 'e')
                    || (source.data[at] == 'E'))) {
                ++at;
                if (is_in_bounds(source, at)
                    && ((source.data[at] == '+')
                        || (source.data[at] == '-'))) {

                    ++at;
                }
                while (is_json_digit(source, at)) {
                    ++at;
                }
            }
            result.value.count = at - start;
        } break;
        default:
            break;
        }
    }
    parser->at = at;
    return result;
}

json_element* parse_json_list(json_parser* parser,
    json_token starting_token,
    eJsonTokenType end_type,
    bool has_labels);

json_element* parse_json_element(json_parser* parser, buffer label, json_token value)
{
    bool valid = true;
    json_element* sub_element = 0;
    if (value.type == eJsonTokenType::OpenBracket) {
        sub_element = parse_json_list(parser, value, eJsonTokenType::CloseBracket, false);
    } else if (value.type == eJsonTokenType::OpenBrace) {
        sub_element = parse_json_list(parser, value, eJsonTokenType::CloseBrace, true);
    } else if (value.type == eJsonTokenType::StringLiteral || value.type == eJsonTokenType::True || value.type == eJsonTokenType::False || value.type == eJsonTokenType::Null || value.type == eJsonTokenType::Number) {
        // nothing to do here
    } else {
        valid = false;
    }

    json_element* result = nullptr;
    if (valid) {
        result = (json_element*)malloc(sizeof(json_element));
        result->label = label;
        result->value = value.value;
        result->first_sub_element = sub_element;
        result->next_sibling = nullptr;
    }
    return result;
}

json_element* parse_json_list(json_parser* parser,
    json_token starting_token,
    eJsonTokenType end_type,
    bool has_labels)
{
    json_element* first_element = {};
    json_element* last_element = {};
    while (is_parsing(parser)) {
        buffer label = {};
        json_token value = get_json_token(parser);
        if (has_labels) {
            if (value.type == eJsonTokenType::StringLiteral) {
                label = value.value;
                json_token colon = get_json_token(parser);
                if (colon.type == eJsonTokenType::Colon)
                    value = get_json_token(parser);
                else
                    error(parser, colon, "Expected colong after field name");
            } else if (value.type != end_type) {
                error(parser, value, "unexpected token in json");
            }
        }
        json_element* element = parse_json_element(parser, label, value);
        if (element) {
            last_element = (last_element ? last_element->next_sibling : first_element) = element;
        } else if (value.type == end_type) {
            break;
        } else {
            error(parser, value, "unexpected token in json");
        }

        json_token comma = get_json_token(parser);
        if (comma.type == end_type) {
            break;
        } else if (comma.type != eJsonTokenType::Comma) {
            error(parser, comma, "unexpected token in json");
        }
    }
    return first_element;
}

json_element* parse_json(buffer input_json)
{
    json_parser parser = {};
    parser.source = input_json;

    json_element* result = parse_json_element(&parser, {}, get_json_token(&parser));
    return result;
}

void free_json(json_element* element)
{
    while (element) {
        json_element* free_element = element;
        element = element->next_sibling;
        free_json(free_element->first_sub_element);
        free(free_element);
    }
}

json_element* lookup_element(json_element* object, buffer element_name)
{
    json_element* result = nullptr;
    if (object) {
        for (json_element* search = object->first_sub_element; search; search = search->next_sibling) {
            if (are_equal(search->label, element_name)) {
                result = search;
                break;
            }
        }
    }
    return result;
}

double convert_json_sign(buffer source, uint64_t* at_result)
{
    uint64_t at = *at_result;
    double result = 1.0;
    if (is_in_bounds(source, at) && (source.data[at] == '-')) {
        result = -1.0;
        ++at;
    }
    *at_result = at;
    return result;
}

double convert_json_number(buffer source, uint64_t* at_result)
{
    uint64_t at = *at_result;
    double result = 0.0;
    while (is_in_bounds(source, at)) {
        uint8_t c = uint8_t(source.data[at]) - (uint8_t)'0';
        if (c < 10) {
            result = 10.0 * result + double(c);
            ++at;
        } else {
            break;
        }
    }
    *at_result = at;
    return result;
}

double convert_element_to_double(json_element* object, buffer element_name)
{
    double result = 0.0;
    json_element* element = lookup_element(object, element_name);
    if (element) {
        buffer source = element->value;
        uint64_t at = 0;

        double sign = convert_json_sign(source, &at);
        double number = convert_json_number(source, &at);
        if (is_in_bounds(source, at) && (source.data[at] == '.')) {
            ++at;
            double C = 1.0 / 10.0;
            while (is_in_bounds(source, at)) {
                uint8_t Char = source.data[at] - (uint8_t)'0';
                if (Char < 10) {
                    number = number + C * (double)Char;
                    C *= 1.0 / 10.0;
                    ++at;
                } else {
                    break;
                }
            }
        }
        if (is_in_bounds(source, at) && ((source.data[at] == 'e') || (source.data[at] == 'E'))) {
            ++at;
            if (is_in_bounds(source, at) && source.data[at] == '+') {
                ++at;
            }
            double exponent_sign = convert_json_sign(source, &at);
            double exponent = exponent_sign * convert_json_number(source, &at);
            number *= std::pow(10.0, exponent);
        }
        result = sign * number;
    }
    return result;
}
uint64_t parse_haversine_pairs(buffer input_json, uint64_t max_pair_count, haversine_pair* pairs)
{
    uint64_t pair_count = 0;
    json_element* json = parse_json(input_json);
    json_element* pairs_array = lookup_element(json, CONSTANT_STRING("pairs"));
    if (pairs_array) {
        for (json_element* element = pairs_array->first_sub_element; element && pair_count < max_pair_count; element = element->next_sibling) {
            haversine_pair* pair = pairs + pair_count++;
            //            std::cout << std::setw(10) << std::fixed;
            pair->x0 = convert_element_to_double(element, CONSTANT_STRING("x0"));
            //            std::cout << "x0: " << pair->x0 << ", ";
            pair->y0 = convert_element_to_double(element, CONSTANT_STRING("y0"));
            //            std::cout << "y0: " << pair->y0 << ", ";
            pair->x1 = convert_element_to_double(element, CONSTANT_STRING("x1"));
            //            std::cout << "x1: " << pair->x1 << ", ";
            pair->y1 = convert_element_to_double(element, CONSTANT_STRING("y1"));
            //            std::cout << "y1: " << pair->y1 << std::endl;
        }
    }
    free_json(json);
    return pair_count;
}
} // namespace json
