/*
 * *****************************************************************************
 *
 * Copyright 2018 Gavin D. Howard
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * *****************************************************************************
 *
 * Code common to the parsers.
 *
 */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>

#include <status.h>
#include <vector.h>
#include <lex.h>
#include <parse.h>
#include <program.h>

BcStatus bc_parse_addFunc(BcParse *p, char *name, size_t *idx) {
	BcStatus s = bc_program_addFunc(p->prog, name, idx);
	p->func = bc_vec_item(&p->prog->fns, p->fidx);
	return s;
}

BcStatus bc_parse_pushName(BcParse *p, char *name) {

	BcStatus s = BC_STATUS_SUCCESS;
	size_t i = 0, len = strlen(name);

	for (; !s && i < len; ++i) s = bc_parse_push(p, (char) name[i]);
	if (s || (s = bc_parse_push(p, BC_PARSE_STREND))) return s;

	free(name);

	return s;
}

BcStatus bc_parse_pushIndex(BcParse *p, size_t idx) {

	BcStatus s;
	unsigned char amt, i, nums[sizeof(size_t)];

	for (amt = 0; idx; ++amt) {
		nums[amt] = (char) idx;
		idx = (idx & ((unsigned long) ~(UCHAR_MAX))) >> sizeof(char) * CHAR_BIT;
	}

	if ((s = bc_parse_push(p, amt))) return s;
	for (i = 0; !s && i < amt; ++i) s = bc_parse_push(p, nums[i]);

	return s;
}

BcStatus bc_parse_number(BcParse *p, BcInst *prev, size_t *nexs) {

	BcStatus s;
	char *num;
	size_t idx = p->prog->consts.len;

	if (!(num = strdup(p->l.t.v.v))) return BC_STATUS_ALLOC_ERR;

	if ((s = bc_vec_push(&p->prog->consts, &num))) {
		free(num);
		return s;
	}

	if ((s = bc_parse_push(p, BC_INST_NUM))) return s;
	if ((s = bc_parse_pushIndex(p, idx))) return s;

	++(*nexs);
	(*prev) = BC_INST_NUM;

	return s;
}

BcStatus bc_parse_text(BcParse *p, const char *text) {

	BcStatus s;

	p->func = bc_vec_item(&p->prog->fns, p->fidx);

	if (!strcmp(text, "") && !BC_PARSE_CAN_EXEC(p)) {
		p->l.t.t = BC_LEX_INVALID;
		if ((s = p->parse(p))) return s;
		if (!BC_PARSE_CAN_EXEC(p)) return BC_STATUS_EXEC_FILE_NOT_EXECUTABLE;
	}

	return bc_lex_text(&p->l, text);
}

BcStatus bc_parse_reset(BcParse *p, BcStatus s) {

	if (p->fidx != BC_PROG_MAIN) {

		p->func->nparams = 0;
		bc_vec_npop(&p->func->code, p->func->code.len);
		bc_vec_npop(&p->func->autos, p->func->autos.len);
		bc_vec_npop(&p->func->labels, p->func->labels.len);

		bc_parse_updateFunc(p, BC_PROG_MAIN);
	}

	p->l.idx = p->l.len;
	p->l.t.t = BC_LEX_EOF;
	p->auto_part = (p->nbraces = 0);

	bc_vec_npop(&p->flags, p->flags.len - 1);
	bc_vec_npop(&p->exits, p->exits.len);
	bc_vec_npop(&p->conds, p->conds.len);
	bc_vec_npop(&p->ops, p->ops.len);

	return bc_program_reset(p->prog, s);
}

void bc_parse_free(BcParse *p) {
	assert(p);
	bc_vec_free(&p->flags);
	bc_vec_free(&p->exits);
	bc_vec_free(&p->conds);
	bc_vec_free(&p->ops);
	bc_lex_free(&p->l);
}

BcStatus bc_parse_create(BcParse *p, BcProgram *prog, size_t func,
                         BcParseParse parse, BcLexNext next)
{
	BcStatus s;

	assert(p && prog);

	memset(p, 0, sizeof(BcParse));

	if ((s = bc_lex_init(&p->l, next))) return s;
	if ((s = bc_vec_init(&p->flags, sizeof(uint8_t), NULL))) goto err;
	if ((s = bc_vec_init(&p->exits, sizeof(BcInstPtr), NULL))) goto err;
	if ((s = bc_vec_init(&p->conds, sizeof(size_t), NULL))) goto err;
	if ((s = bc_vec_pushByte(&p->flags, 0))) goto err;
	if ((s = bc_vec_init(&p->ops, sizeof(BcLexType), NULL))) goto err;

	p->parse = parse;
	p->prog = prog;
	p->auto_part = (p->nbraces = 0);
	bc_parse_updateFunc(p, func);

	return s;

err:
	bc_parse_free(p);
	return s;
}
