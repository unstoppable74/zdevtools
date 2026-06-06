// Copyright 2025 Andrew Apted.
// Use of this code is governed by an MIT-style license.
// See the top-level "LICENSE.md" file for the full text.

/* code parsing */

// this indicates that a parsing function which normally returns
// a Node has failed, and posted an error via PostError().
extern struct Node * P_FAIL;

// this indicates that when evaluating a constant expression, it
// contained a user constant which has not been evaluated yet, or a
// label which has not been resolved yet.
extern struct Node * P_UNKNOWN;

void P_InitParser (void);

struct Node * P_TryParseLabel (struct Node * input);
struct Node * P_ParseLine (struct Node * input);
