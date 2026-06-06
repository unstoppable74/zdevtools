// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* evaluating expressions */

enum EvalContext
{
	ECTX_Constant  = 0,  // a user-defined constant, labels not allowed
	ECTX_Resolve   = 1,  // during resolve pass, labels not finalized
	ECTX_Assemble  = 2,  // during assembly, labels have an address
};

int IsUnaryOperator  (struct Node * T);
int IsBinaryOperator (struct Node * T);

struct Node * Evaluate (struct Node * expr, enum EvalContext ctx, struct FunctionDef * cur_func);

bool ExpressionsInNode (struct Node * U, enum EvalContext ctx, struct FunctionDef * cur_func);
