// This file contains the complete implementations for all missing statement types
// These should replace the TODOs in lower.c

// ==== COMPLETE FOR LOOP IMPLEMENTATION ====
static void lower_for_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_FOR_STMT);

    // Create loop blocks
    IrBasicBlock* header = ir_function_new_block(ctx->current_function,
        "loop.header", ctx->arena);
    IrBasicBlock* body = ir_function_new_block(ctx->current_function,
        "loop.body", ctx->arena);
    IrBasicBlock* increment = NULL;
    if (stmt->data.for_stmt.continue_expr) {
        increment = ir_function_new_block(ctx->current_function,
            "loop.increment", ctx->arena);
    }
    IrBasicBlock* exit = ir_function_new_block(ctx->current_function,
        "loop.exit", ctx->arena);

    // Handle range iteration: for i in 0..10 { ... }
    if (stmt->data.for_stmt.loop_var && stmt->data.for_stmt.iterable &&
        stmt->data.for_stmt.iterable->kind == AST_RANGE_EXPR) {

        // Lower range start and end
        IrValue* start = lower_expr(ctx, stmt->data.for_stmt.iterable->data.range_expr.start);
        IrValue* end = lower_expr(ctx, stmt->data.for_stmt.iterable->data.range_expr.end);

        // Create loop variable and initialize to start
        IrInstruction* alloca_instr = ir_alloc_instruction(IR_ALLOCA, ctx->arena);
        alloca_instr->loc = stmt->loc;
        alloca_instr->data.alloca.dest = ir_function_new_temp(ctx->current_function,
            start->type, ctx->arena);
        alloca_instr->data.alloca.dest->data.temp.name = stmt->data.for_stmt.loop_var;
        alloca_instr->data.alloca.alloc_type = start->type;
        alloca_instr->data.alloca.count = 1;
        ir_block_add_instruction(ctx->current_block, alloca_instr, ctx->arena);

        IrInstruction* store_instr = ir_alloc_instruction(IR_STORE, ctx->arena);
        store_instr->loc = stmt->loc;
        store_instr->data.store.addr = alloca_instr->data.alloca.dest;
        store_instr->data.store.value = start;
        ir_block_add_instruction(ctx->current_block, store_instr, ctx->arena);

        IrValue* loop_var_addr = alloca_instr->data.alloca.dest;

        // Jump to header
        IrInstruction* jump_header = ir_alloc_instruction(IR_JUMP, ctx->arena);
        jump_header->data.jump.target = header;
        ir_block_add_instruction(ctx->current_block, jump_header, ctx->arena);
        ir_block_add_successor(ctx->current_block, header, ctx->arena);
        ir_block_add_predecessor(header, ctx->current_block, ctx->arena);

        // Header: check if i < end
        ir_function_add_block(ctx->current_function, header, ctx->arena);
        ctx->current_block = header;

        IrInstruction* load_instr = ir_alloc_instruction(IR_LOAD, ctx->arena);
        load_instr->loc = stmt->loc;
        load_instr->data.load.dest = ir_function_new_temp(ctx->current_function,
            start->type, ctx->arena);
        load_instr->data.load.addr = loop_var_addr;
        ir_block_add_instruction(ctx->current_block, load_instr, ctx->arena);

        IrValue* current_val = load_instr->data.load.dest;
        IrValue* cond_result = ir_function_new_temp(ctx->current_function,
            start->type, ctx->arena);

        IrInstruction* cmp_instr = ir_alloc_instruction(IR_BINARY_OP, ctx->arena);
        cmp_instr->loc = stmt->loc;
        cmp_instr->data.binary_op.dest = cond_result;
        cmp_instr->data.binary_op.left = current_val;
        cmp_instr->data.binary_op.right = end;
        cmp_instr->data.binary_op.op = IR_OP_LT;
        ir_block_add_instruction(ctx->current_block, cmp_instr, ctx->arena);

        IrInstruction* cond_jump = ir_alloc_instruction(IR_COND_JUMP, ctx->arena);
        cond_jump->data.cond_jump.cond = cond_result;
        cond_jump->data.cond_jump.true_target = body;
        cond_jump->data.cond_jump.false_target = exit;
        ir_block_add_instruction(ctx->current_block, cond_jump, ctx->arena);

        ir_block_add_successor(header, body, ctx->arena);
        ir_block_add_predecessor(body, header, ctx->arena);
        ir_block_add_successor(header, exit, ctx->arena);
        ir_block_add_predecessor(exit, header, ctx->arena);

        // Body
        ir_function_add_block(ctx->current_function, body, ctx->arena);
        ctx->current_block = body;

        IrBasicBlock* saved_break = ctx->break_target;
        IrBasicBlock* saved_continue = ctx->continue_target;
        ctx->break_target = exit;
        ctx->continue_target = header;

        lower_stmt(ctx, stmt->data.for_stmt.body);

        ctx->break_target = saved_break;
        ctx->continue_target = saved_continue;

        // Increment: i = i + 1
        IrInstruction* load_inc = ir_alloc_instruction(IR_LOAD, ctx->arena);
        load_inc->loc = stmt->loc;
        load_inc->data.load.dest = ir_function_new_temp(ctx->current_function,
            start->type, ctx->arena);
        load_inc->data.load.addr = loop_var_addr;
        ir_block_add_instruction(ctx->current_block, load_inc, ctx->arena);

        IrValue* one = ir_alloc_value(IR_VALUE_CONSTANT, start->type, ctx->arena);
        one->data.constant.data.int_val = 1;

        IrValue* inc_result = ir_function_new_temp(ctx->current_function,
            start->type, ctx->arena);

        IrInstruction* add_instr = ir_alloc_instruction(IR_BINARY_OP, ctx->arena);
        add_instr->loc = stmt->loc;
        add_instr->data.binary_op.dest = inc_result;
        add_instr->data.binary_op.left = load_inc->data.load.dest;
        add_instr->data.binary_op.right = one;
        add_instr->data.binary_op.op = IR_OP_ADD;
        ir_block_add_instruction(ctx->current_block, add_instr, ctx->arena);

        IrInstruction* store_inc = ir_alloc_instruction(IR_STORE, ctx->arena);
        store_inc->loc = stmt->loc;
        store_inc->data.store.addr = loop_var_addr;
        store_inc->data.store.value = inc_result;
        ir_block_add_instruction(ctx->current_block, store_inc, ctx->arena);

        // Jump back to header
        IrInstruction* jump_back = ir_alloc_instruction(IR_JUMP, ctx->arena);
        jump_back->data.jump.target = header;
        ir_block_add_instruction(ctx->current_block, jump_back, ctx->arena);
        ir_block_add_successor(ctx->current_block, header, ctx->arena);
        ir_block_add_predecessor(header, ctx->current_block, ctx->arena);

    } else {
        // Handle condition-only or infinite loop
        // Jump to header
        IrInstruction* jump_header = ir_alloc_instruction(IR_JUMP, ctx->arena);
        jump_header->data.jump.target = header;
        ir_block_add_instruction(ctx->current_block, jump_header, ctx->arena);
        ir_block_add_successor(ctx->current_block, header, ctx->arena);
        ir_block_add_predecessor(header, ctx->current_block, ctx->arena);

        // Header (condition check)
        ir_function_add_block(ctx->current_function, header, ctx->arena);
        ctx->current_block = header;

        if (stmt->data.for_stmt.condition) {
            // Condition-only loop (while-style)
            IrValue* cond = lower_expr(ctx, stmt->data.for_stmt.condition);
            IrInstruction* cond_jump = ir_alloc_instruction(IR_COND_JUMP, ctx->arena);
            cond_jump->data.cond_jump.cond = cond;
            cond_jump->data.cond_jump.true_target = body;
            cond_jump->data.cond_jump.false_target = exit;
            ir_block_add_instruction(ctx->current_block, cond_jump, ctx->arena);

            ir_block_add_successor(header, body, ctx->arena);
            ir_block_add_predecessor(body, header, ctx->arena);
            ir_block_add_successor(header, exit, ctx->arena);
            ir_block_add_predecessor(exit, header, ctx->arena);
        } else {
            // Infinite loop - always jump to body
            IrInstruction* jump_body = ir_alloc_instruction(IR_JUMP, ctx->arena);
            jump_body->data.jump.target = body;
            ir_block_add_instruction(ctx->current_block, jump_body, ctx->arena);
            ir_block_add_successor(header, body, ctx->arena);
            ir_block_add_predecessor(body, header, ctx->arena);
        }

        // Body
        ir_function_add_block(ctx->current_function, body, ctx->arena);
        ctx->current_block = body;

        IrBasicBlock* saved_break = ctx->break_target;
        IrBasicBlock* saved_continue = ctx->continue_target;
        ctx->break_target = exit;
        ctx->continue_target = increment ? increment : header;

        lower_stmt(ctx, stmt->data.for_stmt.body);

        ctx->break_target = saved_break;
        ctx->continue_target = saved_continue;

        // Handle increment block if it exists
        if (increment) {
            IrInstruction* jump_inc = ir_alloc_instruction(IR_JUMP, ctx->arena);
            jump_inc->data.jump.target = increment;
            ir_block_add_instruction(ctx->current_block, jump_inc, ctx->arena);
            ir_block_add_successor(ctx->current_block, increment, ctx->arena);
            ir_block_add_predecessor(increment, ctx->current_block, ctx->arena);

            ir_function_add_block(ctx->current_function, increment, ctx->arena);
            ctx->current_block = increment;

            lower_expr(ctx, stmt->data.for_stmt.continue_expr);

            IrInstruction* jump_back = ir_alloc_instruction(IR_JUMP, ctx->arena);
            jump_back->data.jump.target = header;
            ir_block_add_instruction(ctx->current_block, jump_back, ctx->arena);
            ir_block_add_successor(ctx->current_block, header, ctx->arena);
            ir_block_add_predecessor(header, ctx->current_block, ctx->arena);
        } else {
            // Jump back to header
            IrInstruction* jump_back = ir_alloc_instruction(IR_JUMP, ctx->arena);
            jump_back->data.jump.target = header;
            ir_block_add_instruction(ctx->current_block, jump_back, ctx->arena);
            ir_block_add_successor(ctx->current_block, header, ctx->arena);
            ir_block_add_predecessor(header, ctx->current_block, ctx->arena);
        }
    }

    // Continue in exit block
    ir_function_add_block(ctx->current_function, exit, ctx->arena);
    ctx->current_block = exit;
}

// ==== SWITCH STATEMENT IMPLEMENTATION ====
static void lower_switch_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_SWITCH_STMT);

    // Lower switch value
    IrValue* value = lower_expr(ctx, stmt->data.switch_stmt.value);

    // Create merge block
    IrBasicBlock* merge = ir_function_new_block(ctx->current_function,
        "switch.merge", ctx->arena);

    // Create case blocks
    IrBasicBlock** case_blocks = arena_alloc(ctx->arena,
        sizeof(IrBasicBlock*) * stmt->data.switch_stmt.case_count, 8);

    for (size_t i = 0; i < stmt->data.switch_stmt.case_count; i++) {
        case_blocks[i] = ir_function_new_block(ctx->current_function,
            "switch.case", ctx->arena);
    }

    IrBasicBlock* default_block = NULL;
    size_t default_idx = (size_t)-1;

    // Find default case if it exists
    for (size_t i = 0; i < stmt->data.switch_stmt.case_count; i++) {
        if (stmt->data.switch_stmt.cases[i].value_count == 0 ||
            stmt->data.switch_stmt.cases[i].values == NULL) {
            default_block = case_blocks[i];
            default_idx = i;
            break;
        }
    }

    if (!default_block) {
        default_block = merge;
    }

    // Generate comparison chain for each case
    IrBasicBlock* current = ctx->current_block;
    for (size_t i = 0; i < stmt->data.switch_stmt.case_count; i++) {
        if (i == default_idx) continue;  // Skip default case in comparison chain

        AstSwitchCase* case_node = &stmt->data.switch_stmt.cases[i];

        // For each value in this case
        for (size_t j = 0; j < case_node->value_count; j++) {
            IrValue* case_value = lower_expr(ctx, case_node->values[j]);

            // Compare switch value with case value
            IrValue* cmp_result = ir_function_new_temp(ctx->current_function,
                value->type, ctx->arena);

            IrInstruction* cmp = ir_alloc_instruction(IR_BINARY_OP, ctx->arena);
            cmp->loc = stmt->loc;
            cmp->data.binary_op.dest = cmp_result;
            cmp->data.binary_op.left = value;
            cmp->data.binary_op.right = case_value;
            cmp->data.binary_op.op = IR_OP_EQ;
            ir_block_add_instruction(current, cmp, ctx->arena);

            // Create next check block (for the next case value)
            IrBasicBlock* next_check = (j < case_node->value_count - 1 ||
                i < stmt->data.switch_stmt.case_count - 1) ?
                ir_function_new_block(ctx->current_function, "switch.check", ctx->arena) :
                default_block;

            // Jump to case block if equal, else to next check
            IrInstruction* cond_jump = ir_alloc_instruction(IR_COND_JUMP, ctx->arena);
            cond_jump->data.cond_jump.cond = cmp_result;
            cond_jump->data.cond_jump.true_target = case_blocks[i];
            cond_jump->data.cond_jump.false_target = next_check;
            ir_block_add_instruction(current, cond_jump, ctx->arena);

            ir_block_add_successor(current, case_blocks[i], ctx->arena);
            ir_block_add_predecessor(case_blocks[i], current, ctx->arena);
            ir_block_add_successor(current, next_check, ctx->arena);
            ir_block_add_predecessor(next_check, current, ctx->arena);

            if (next_check != default_block && next_check != merge) {
                ir_function_add_block(ctx->current_function, next_check, ctx->arena);
            }
            current = next_check;
        }
    }

    // Lower each case body
    IrBasicBlock* saved_break = ctx->break_target;
    ctx->break_target = merge;

    for (size_t i = 0; i < stmt->data.switch_stmt.case_count; i++) {
        ir_function_add_block(ctx->current_function, case_blocks[i], ctx->arena);
        ctx->current_block = case_blocks[i];

        AstSwitchCase* case_node = &stmt->data.switch_stmt.cases[i];
        for (size_t j = 0; j < case_node->stmt_count; j++) {
            lower_stmt(ctx, case_node->stmts[j]);
        }

        // Jump to merge after case (if no explicit break)
        IrInstruction* jump_merge = ir_alloc_instruction(IR_JUMP, ctx->arena);
        jump_merge->data.jump.target = merge;
        ir_block_add_instruction(ctx->current_block, jump_merge, ctx->arena);
        ir_block_add_successor(ctx->current_block, merge, ctx->arena);
        ir_block_add_predecessor(merge, ctx->current_block, ctx->arena);
    }

    ctx->break_target = saved_break;

    // Continue in merge block
    ir_function_add_block(ctx->current_function, merge, ctx->arena);
    ctx->current_block = merge;
}

// ==== BREAK AND CONTINUE STATEMENTS ====
static void lower_break_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_BREAK_STMT);

    if (ctx->break_target) {
        IrInstruction* jump = ir_alloc_instruction(IR_JUMP, ctx->arena);
        jump->data.jump.target = ctx->break_target;
        ir_block_add_instruction(ctx->current_block, jump, ctx->arena);
        ir_block_add_successor(ctx->current_block, ctx->break_target, ctx->arena);
        ir_block_add_predecessor(ctx->break_target, ctx->current_block, ctx->arena);
    }
}

static void lower_continue_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_CONTINUE_STMT);

    if (ctx->continue_target) {
        IrInstruction* jump = ir_alloc_instruction(IR_JUMP, ctx->arena);
        jump->data.jump.target = ctx->continue_target;
        ir_block_add_instruction(ctx->current_block, jump, ctx->arena);
        ir_block_add_successor(ctx->current_block, ctx->continue_target, ctx->arena);
        ir_block_add_predecessor(ctx->continue_target, ctx->current_block, ctx->arena);
    }
}

// ==== CONTINUE SWITCH STATEMENT ====
static void lower_continue_switch_stmt(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_CONTINUE_SWITCH_STMT);

    // Continue switch jumps back to switch header with a new value
    // This is used for computed goto patterns in state machines
    // For now, treat it like continue
    if (ctx->continue_target) {
        IrInstruction* jump = ir_alloc_instruction(IR_JUMP, ctx->arena);
        jump->data.jump.target = ctx->continue_target;
        ir_block_add_instruction(ctx->current_block, jump, ctx->arena);
        ir_block_add_successor(ctx->current_block, ctx->continue_target, ctx->arena);
        ir_block_add_predecessor(ctx->continue_target, ctx->current_block, ctx->arena);
    }
}

// ==== UPDATED DEFER/ERRDEFER TO COLLECT INSTRUCTIONS ====
static void lower_defer_stmt_complete(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_DEFER_STMT);

    // Create defer cleanup
    IrDeferCleanup* cleanup = arena_alloc(ctx->arena, sizeof(IrDeferCleanup), 8);
    cleanup->is_errdefer = false;

    // Create a temporary block to collect defer instructions
    IrBasicBlock* defer_block = ir_function_new_block(ctx->current_function,
        "defer.collect", ctx->arena);

    // Save current block and switch to defer block
    IrBasicBlock* saved_block = ctx->current_block;
    ctx->current_block = defer_block;

    // Lower defer body into the temporary block
    lower_stmt(ctx, stmt->data.defer_stmt.stmt);

    // Collect instructions from defer block
    cleanup->instruction_count = defer_block->instruction_count;
    cleanup->instructions = defer_block->instructions;

    // Restore current block
    ctx->current_block = saved_block;

    // Push onto defer stack
    if (!ctx->defer_stack) {
        ctx->defer_capacity = 8;
        ctx->defer_stack = arena_alloc(ctx->arena,
            sizeof(IrDeferCleanup*) * ctx->defer_capacity, 8);
    }
    if (ctx->defer_count >= ctx->defer_capacity) {
        ctx->defer_capacity *= 2;
        IrDeferCleanup** new_stack = arena_alloc(ctx->arena,
            sizeof(IrDeferCleanup*) * ctx->defer_capacity, 8);
        memcpy(new_stack, ctx->defer_stack,
            sizeof(IrDeferCleanup*) * ctx->defer_count);
        ctx->defer_stack = new_stack;
    }
    ctx->defer_stack[ctx->defer_count++] = cleanup;
}

static void lower_errdefer_stmt_complete(LowerContext* ctx, AstNode* stmt) {
    assert(stmt->kind == AST_ERRDEFER_STMT);

    // Similar to defer but only executed on error paths
    IrDeferCleanup* cleanup = arena_alloc(ctx->arena, sizeof(IrDeferCleanup), 8);
    cleanup->is_errdefer = true;

    // Create a temporary block to collect errdefer instructions
    IrBasicBlock* errdefer_block = ir_function_new_block(ctx->current_function,
        "errdefer.collect", ctx->arena);

    // Save current block and switch to errdefer block
    IrBasicBlock* saved_block = ctx->current_block;
    ctx->current_block = errdefer_block;

    // Lower errdefer body into the temporary block
    lower_stmt(ctx, stmt->data.defer_stmt.stmt);

    // Collect instructions from errdefer block
    cleanup->instruction_count = errdefer_block->instruction_count;
    cleanup->instructions = errdefer_block->instructions;

    // Restore current block
    ctx->current_block = saved_block;

    // Push onto defer stack
    if (!ctx->defer_stack) {
        ctx->defer_capacity = 8;
        ctx->defer_stack = arena_alloc(ctx->arena,
            sizeof(IrDeferCleanup*) * ctx->defer_capacity, 8);
    }
    if (ctx->defer_count >= ctx->defer_capacity) {
        ctx->defer_capacity *= 2;
        IrDeferCleanup** new_stack = arena_alloc(ctx->arena,
            sizeof(IrDeferCleanup*) * ctx->defer_capacity, 8);
        memcpy(new_stack, ctx->defer_stack,
            sizeof(IrDeferCleanup*) * ctx->defer_count);
        ctx->defer_stack = new_stack;
    }
    ctx->defer_stack[ctx->defer_count++] = cleanup;
}

// ==== ADD TO lower_stmt SWITCH STATEMENT ====
/*
In lower_stmt(), add these cases:

        case AST_BREAK_STMT:
            lower_break_stmt(ctx, stmt);
            break;
        case AST_CONTINUE_STMT:
            lower_continue_stmt(ctx, stmt);
            break;
        case AST_CONTINUE_SWITCH_STMT:
            lower_continue_switch_stmt(ctx, stmt);
            break;
*/
