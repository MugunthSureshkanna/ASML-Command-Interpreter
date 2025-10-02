#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "parser.h"
#include <string.h>

#include "command_type.h"
#include "token_type.h"

static Token    advance(Parser *parser);
static bool     consume(Parser *parser, TokenType type);
static bool     is_at_end(Parser *parser);
static void     skip_nls(Parser *parser);
static bool     consume_newline(Parser *parser);
static Command *create_command(CommandType type);
static bool     is_variable(Token token);
static bool     parse_variable(Token token, int64_t *var_num);
static bool     parse_number(Token token, int64_t *result);
static bool     parse_variable_operand(Parser *parser, Operand *op);
static bool     parse_var_or_imm(Parser *parser, Operand *op, bool *is_immediate);
static Command *parse_cmd(Parser *parser);

void parser_init(Parser *parser, Lexer *lexer, LabelMap *map) {
    if (!parser) {
        return;
    }

    parser->lexer     = lexer;
    parser->had_error = false;
    parser->label_map = map;
    parser->current   = lexer_next_token(parser->lexer);
    parser->next      = lexer_next_token(parser->lexer);
}

/**
 * @brief Advances the parser in the token stream.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return The token that was just consumed.
 */
static Token advance(Parser *parser) {
    Token ret_token = parser->current;
    if (!is_at_end(parser)) {
        parser->current = parser->next;
        parser->next    = lexer_next_token(parser->lexer);
    }
    return ret_token;
}

/**
 * @brief Determines if the parser reached the end of the token stream.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return True if the parser is at the end of the token stream, false
 * otherwise.
 */
static bool is_at_end(Parser *parser) {
    return parser->current.type == TOK_EOF;
}

/**
 * @brief Consumes the token if it matches the specified token type.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param type The type of the token to match.
 * @return True if the token was consumed, false otherwise.
 */
static bool consume(Parser *parser, TokenType type) {
    if (parser->current.type == type) {
        advance(parser);
        return true;
    }

    return false;
}

/**
 * @brief Creates a command of the given type.
 *
 * @param type The type of the command to create.
 * @return A pointer to a command with the requested type.
 *
 * @note It is the responsibility of the caller to free the memory associated
 * with the returned command.
 */
static Command *create_command(CommandType type) {
    Command *cmd = (Command *) calloc(1, sizeof(Command));
    if (!cmd) {
        return NULL;
    }

    cmd->type             = type;
    cmd->next             = NULL;
    cmd->is_a_immediate   = false;
    cmd->is_a_string      = false;
    cmd->is_b_immediate   = false;
    cmd->is_b_string      = false;
    cmd->branch_condition = BRANCH_NONE;
    return cmd;
}

/**
 * @brief Determines if the given token is a valid variable.
 *
 * A valid (potential) variable is a token that begins with the prefix "x",
 * followed by any other character(s).
 *
 * @param token The token to check.
 * @return True if this token could be a variable, false otherwise.
 */
static bool is_variable(Token token) {
    return token.length >= 2 && token.lexeme[0] == 'x';
}

/**
 * @brief Determines if the given token is a valid base signifier.
 *
 * A valid base signifier is one of d (decimal), x (hex), b (binary) or s (string).
 *
 * @param token The token to check.
 * @return True if this token is a base signifier, false otherwise
 */
static bool is_base(Token token) {
    return token.length == 1 && (token.lexeme[0] == 'd' || token.lexeme[0] == 'x' ||
                                 token.lexeme[0] == 's' || token.lexeme[0] == 'b');
}

/**
 * @brief Parses the given token as a base signifier
 *
 * A base is a single character, either d, s, x, or b.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @return True if the current token was parsed as a base, false otherwise.
 */
static bool parse_base(Parser *parser, Operand *op) {
    // STUDENT TODO: Parse the current token as a base
    if (!is_base(parser->current)) {
        return false;
    }
    op->base = parser->current.lexeme[0]; 
    advance(parser); 
    return true;
}

/**
 * @brief Parses the given token as a variable.
 *
 * @param token The token to parse.
 * @param var_num a pointer to modify on success.
 * @return True if `var_num` was successfully modified, false otherwise.
 *
 * @note It is assumed that the token already was verified to begin with a valid
 * prefix, "x".
 */
static bool parse_variable(Token token, int64_t *var_num) {
    char   *endptr;
    int64_t tempnum = strtol(token.lexeme + 1, &endptr, 10);

    if ((token.lexeme + token.length) != endptr || tempnum < 0 || tempnum > 31) {
        return false;
    }

    *var_num = tempnum;
    return true;
}

/**
 * @brief Parses the given value as a number.
 *
 * @param token The token to parse.
 * @param result A pointer to the value to modify on success.
 * @return True if `result` was successfully modified, false otherwise.
 */
static bool parse_number(Token token, int64_t *result) {
    const char *parse_start = token.lexeme;
    int         base        = 10;

    if (token.length > 2 && token.lexeme[0] == '0') {
        if (token.lexeme[1] == 'x') {
            parse_start += 2;
            base = 16;
        } else if (token.lexeme[1] == 'b') {
            parse_start += 2;
            base = 2;
        }
    }

    char *endptr;
    *result = strtoll(parse_start, &endptr, base);

    return (token.lexeme + token.length) == endptr;
}

/**
 * @brief Conditionally parses the current token as a number.
 *
 * Note that this won't advance the parser if the token cannot be converted to
 * an integer.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @return True if this token is a number and was was converted successfully,
 * false otherwise.
 */
static bool parse_im(Parser *parser, Operand *op) {
    // STUDENT TODO: Parse current token as an immediate
    if (parser->current.type != TOK_NUM) {
        return false;
    }
    int64_t result;
    if (!parse_number(parser->current, &result)) {
        return false;
    }
    op->num_val = result;
    return true;
}

/**
 * @brief Parses the next token as a variable.
 *
 * A variable is anything starting with the prefix x and will be of type
 * TOK_IDENT.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @return True if this was parsed as a variable, false otherwise.
 */
static bool parse_variable_operand(Parser *parser, Operand *op) {
    // STUDENT TODO: Parse the current token as a variable
    if (parser->current.type != TOK_IDENT || !is_variable(parser->current)) {
        return false;
    }
    int64_t var_num;
    if (!parse_variable(parser->current, &var_num)) {
        return false;
    }
    op->num_val = var_num; 
    advance(parser);   
    return true;
}

/**
 * @brief Parses the next token as either a variable or an immediate.
 *
 * A number is considered to be anything beginning with a decimal digit or the
 * prefixes 0b or 0x and will be of type TOK_NUM. A variable is anything
 * starting with the prefix x and will be of type TOK_IDENT.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @param is_immediate A pointer to a boolean to modify upon determining whether
 * the given value is an immediate.
 * @return True if this was parsed as an immediate or a variable, false
 * otherwise.
 */
static bool parse_var_or_imm(Parser *parser, Operand *op, bool *is_immediate) {
    // STUDENT TODO: Parse the current token as a variable or an immediate
    // Check if token is a variable
    if (parser->current.type == TOK_IDENT && is_variable(parser->current)) {
        if (!parse_variable_operand(parser, op)) {
            return false;
        }
        *is_immediate = false; 
        return true;
    }
    // Check if token is immediate
    if (parser->current.type == TOK_NUM) {
        if (!parse_im(parser, op)) {
            return false;
        }
        *is_immediate = true; 
        advance(parser);     
        return true;
    }
    // Not variable or immediate
    return false;
}

/**
 * @brief Skips past tokens that signal the start of a new line
 *
 * Consumes newlines until the end of file is reached.
 * An EOF is not considered to be a "new line" in this context because it is a
 * sentinel token, I.e, there is nothing after it.
 *
 * @param parser A pointer to the parser to read tokens from.
 */
static void skip_nls(Parser *parser) {
    while (consume(parser, TOK_NL))
        ;
}

/**
 * @brief Consumes a single newline
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return True whether a "new line" was consumed, false otherwise.
 *
 * @note An encounter of TOK_EOF should not be considered a failure, as this
 * procedure is designed to "reset" the grammar. In other words, it should be
 * used to ensure that we have a valid ending token after encountering an
 * instruction. Since TOK_EOF signals no more possible instructions, it should
 * effectively play the role of a new line when checking for a valid ending
 * sequence for a command.
 */
static bool consume_newline(Parser *parser) {
    return consume(parser, TOK_NL) || consume(parser, TOK_EOF);
}

/**
 * @brief Parses a singular command.
 *
 * Reads in the token(s) from the lexer that the parser owns and determines the
 * appropriate matching command. Updates the parser->had_error if an error
 * occurs.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return A pointer to the appropriate command.
 * Returns null if an error occurred or there are no commands to parse.
 *
 * @note The caller is responsible for freeing the memory associated with the
 * returned command.
 */
static Command *parse_cmd(Parser *parser) {
    // STUDENT TODO: Parse an individual command by looking at the current token's type
    // TODO: Skip newlines before anything else
    skip_nls(parser);

    // You will need to modify this later
    // However, this is fine for getting going
    Token token = parser->current;

    if (token.type == TOK_IDENT && parser->next.type == TOK_COLON) {
        // Get label name
        Token label_token = token;
        advance(parser); 
        advance(parser);  
        Command *labeled_cmd = parse_cmd(parser);
        // Store label mapping
        if (labeled_cmd) {
            char *label = strndup(label_token.lexeme, label_token.length);
            if (!put_label(parser->label_map, label, labeled_cmd)) {
                parser->had_error = true;
            }
            free(label);
        }
        return labeled_cmd; 
    }

    if (token.type == TOK_EOF) {
        // No commands to parse; we are done
        return NULL;
    }

    Command *cmd = NULL;
    switch (token.type) {
        // STUDENT TODO: Add cases handling different commands
        case TOK_MOV: {
            cmd = create_command(CMD_MOV);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }
            advance(parser); 
            // Parse destination operand
            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            skip_nls(parser);
            // Parse source operand
            if (!parse_im(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            cmd->is_a_immediate = true;
            advance(parser);
            break;
        }
        case TOK_ADD:
        case TOK_SUB: {
            cmd = create_command(token.type == TOK_ADD ? CMD_ADD : CMD_SUB);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }
            advance(parser);
            // Parse destination operand
            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            // Parse first operand
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            // Parse second operand
            if (!parse_var_or_imm(parser, &cmd->val_b, &cmd->is_b_immediate)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            break;
        }
        case TOK_CMP:
        case TOK_CMP_U: {
            cmd = create_command(token.type == TOK_CMP ? CMD_CMP : CMD_CMP_U);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }
            advance(parser); 
            // Parse first operand
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            // Parse second operand
            if (!parse_var_or_imm(parser, &cmd->val_b, &cmd->is_b_immediate)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            break;
        }
        case TOK_PRINT: {
            cmd = create_command(CMD_PRINT);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }
            advance(parser);
            // Parse operand
            if (!parse_var_or_imm(parser, &cmd->val_a, &cmd->is_a_immediate)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            // Parse base
            if (!parse_base(parser, &cmd->val_b)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            break;
        }
        case TOK_AND:
        case TOK_EOR:
        case TOK_ORR: {
            CommandType cmd_type = (token.type == TOK_AND) ? CMD_AND : 
                                   (token.type == TOK_EOR) ? CMD_EOR : CMD_ORR;
            cmd = create_command(cmd_type);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }
            advance(parser);
            // Parse destination operand
            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            // Parse first operand
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            // Parse second operand
            if (!parse_variable_operand(parser, &cmd->val_b)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            cmd->is_a_immediate = false;
            cmd->is_b_immediate = false;
            break;
        }
        case TOK_LSL:
        case TOK_LSR:
        case TOK_ASR: {
            CommandType cmd_type = (token.type == TOK_LSL) ? CMD_LSL : 
                                   (token.type == TOK_LSR) ? CMD_LSR : CMD_ASR;
            cmd = create_command(cmd_type);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }
            advance(parser);
            // Parse destination operand
            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            // value to shift
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            // shift amount
            if (!parse_var_or_imm(parser, &cmd->val_b, &cmd->is_b_immediate)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            break;
        }
        case TOK_LOAD: {
            cmd = create_command(CMD_LOAD);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }
            advance(parser);
            // Parse destination variable
            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            // Parse number of bytes
            if (!parse_im(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            cmd->is_a_immediate = true;
            advance(parser);
            // Parse source address (can be a variable or an immediate)
            if (!parse_var_or_imm(parser, &cmd->val_b, &cmd->is_b_immediate)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            break;
        }
        case TOK_STORE: {
            cmd = create_command(CMD_STORE);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }
            advance(parser);
            // Parse source variable
            if (!parse_variable_operand(parser, &cmd->destination)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            // Parse destination address 
            if (!parse_var_or_imm(parser, &cmd->val_b, &cmd->is_b_immediate)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            // Parse number of bytes 
            if (!parse_im(parser, &cmd->val_a)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            cmd->is_a_immediate = true;
            advance(parser);
            break;
        }
        case TOK_PUT: {
            cmd = create_command(CMD_PUT);
            if (!cmd) {
                parser->had_error = true;
                return NULL;
            }
            advance(parser);
            // Parse string literal
            if (parser->current.type != TOK_STR) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            cmd->val_a.str_val = malloc(parser->current.length + 1);
            if (!cmd->val_a.str_val) {
                parser->had_error = true;
                return NULL;
            }
            memcpy(cmd->val_a.str_val, parser->current.lexeme, parser->current.length);
            cmd->val_a.str_val[parser->current.length] = '\0'; // null-terminate string
            cmd->is_a_string = true;
            advance(parser);
            // Parse destination address 
            if (!parse_var_or_imm(parser, &cmd->val_b, &cmd->is_b_immediate)) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            break;
        }
        case TOK_BRANCH:
        case TOK_BRANCH_EQ:
        case TOK_BRANCH_GE:
        case TOK_BRANCH_GT:
        case TOK_BRANCH_LE:
        case TOK_BRANCH_LT:
        case TOK_BRANCH_NEQ: {
            cmd = create_command(CMD_BRANCH);
            switch (token.type) {
                case TOK_BRANCH:    cmd->branch_condition = BRANCH_ALWAYS; break;
                case TOK_BRANCH_EQ: cmd->branch_condition = BRANCH_EQUAL; break;
                case TOK_BRANCH_GE: cmd->branch_condition = BRANCH_GREATER_EQUAL; break;
                case TOK_BRANCH_GT: cmd->branch_condition = BRANCH_GREATER; break;
                case TOK_BRANCH_LE: cmd->branch_condition = BRANCH_LESS_EQUAL; break;
                case TOK_BRANCH_LT: cmd->branch_condition = BRANCH_LESS; break;
                case TOK_BRANCH_NEQ: cmd->branch_condition = BRANCH_NOT_EQUAL; break;
                default: break;
            }
            advance(parser); 
            if (parser->current.type != TOK_IDENT) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            cmd->val_a.str_val = strndup(parser->current.lexeme, parser->current.length);
            cmd->is_a_string = true;
            advance(parser); 
            break;
        }
        case TOK_CALL: {
            cmd = create_command(CMD_CALL);
            advance(parser); 
            if (parser->current.type != TOK_IDENT) {
                parser->had_error = true;
                free_command(cmd);
                return NULL;
            }
            cmd->val_a.str_val = strndup(parser->current.lexeme, parser->current.length);
            cmd->is_a_string = true;
            advance(parser);
            break;
        }
        case TOK_RET: {
            cmd = create_command(CMD_RET);
            advance(parser); 
            break;
        }
        default:
            parser->had_error = true;
            break;
    }
    // TODO: Check for errors and consume newlines
    if (!consume_newline(parser)) {
        parser->had_error = true;
        free_command(cmd);
        return NULL;
    }
    return cmd;
}

Command *parse_commands(Parser *parser) {
    Command *head = NULL, **tail_ptr = &head; 
    while (!parser->had_error && !is_at_end(parser)) {
        Command *cmd = parse_cmd(parser);
        if (!cmd) break;

        *tail_ptr = cmd;
        tail_ptr = &cmd->next;
    }
    return head;
}