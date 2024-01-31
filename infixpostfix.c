#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SIZE 100

// Stack implementation
typedef struct {
    int top;
    int items[MAX_SIZE];
} Stack;

void push(Stack* stack, int item) {
    if (stack->top == MAX_SIZE - 1) {
        printf("Stack Overflow\n");
        exit(EXIT_FAILURE);
    }
    stack->items[++stack->top] = item;
}

int pop(Stack* stack) {
    if (stack->top == -1) {
        printf("Stack Underflow\n");
        exit(EXIT_FAILURE);
    }
    return stack->items[stack->top--];
}

int isOperand(char ch) {
    return (ch >= '0' && ch <= '9');
}

int evaluate(char operator, int operand1, int operand2) {
    switch (operator) {
        case '+':
            return operand1 + operand2;
        case '-':
            return operand1 - operand2;
        case '*':
            return operand1 * operand2;
        case '/':
            return operand1 / operand2;
        // Handle other operators if needed
        default:
            return 0; // Unknown operator, return 0
    }
}

int evaluatePostfix(char postfix[]) {
    Stack stack;
    stack.top = -1;

    for (int i = 0; postfix[i] != '\0'; i++) {
        if (isOperand(postfix[i])) {
            int operand = 0;
            while (isOperand(postfix[i])) {
                operand = operand * 10 + (postfix[i] - '0');
                i++;
            }
            push(&stack, operand);
            i--; // Move back one position to handle the character after the multi-digit number
        } else if (postfix[i] != ' ') {
            int operand2 = pop(&stack);
            int operand1 = pop(&stack);
            push(&stack, evaluate(postfix[i], operand1, operand2));
        }
    }

    int result = pop(&stack);

    if (stack.top != -1) {
        printf("Invalid postfix expression\n");
        exit(EXIT_FAILURE);
    }

    return result;
}

void infixToPostfix(char infix[], char postfix[]) {
    Stack stack;
    stack.top = -1;

    int i, j;
    for (i = 0, j = 0; infix[i] != '\0'; i++) {
        if (isOperand(infix[i])) {
            while (isOperand(infix[i])) {
                postfix[j++] = infix[i++];
            }
            postfix[j++] = ' '; // Add a space to separate multi-digit numbers
            i--; // Move back one position to handle the character after the multi-digit number
        } else if (infix[i] == '(') {
            push(&stack, infix[i]);
        } else if (infix[i] == ')') {
            while (stack.top != -1 && stack.items[stack.top] != '(') {
                postfix[j++] = stack.items[stack.top--];
                postfix[j++] = ' ';
            }
            if (stack.top != -1 && stack.items[stack.top] == '(') {
                pop(&stack);  // Pop the '(' from the stack
            }
        } else {
            while (stack.top != -1 && (stack.items[stack.top] == '*' || stack.items[stack.top] == '/')) {
                postfix[j++] = stack.items[stack.top--];
                postfix[j++] = ' ';
            }
            push(&stack, infix[i]);
        }
    }

    while (stack.top != -1) {
        postfix[j++] = stack.items[stack.top--];
        postfix[j++] = ' ';
    }

    postfix[j] = '\0';
}

int main() {
    char infix[MAX_SIZE], postfix[MAX_SIZE];

    printf("Enter an infix expression: ");
    fgets(infix, MAX_SIZE, stdin);

    // Remove the newline character from the input buffer
    if (strlen(infix) > 0 && infix[strlen(infix) - 1] == '\n') {
        infix[strlen(infix) - 1] = '\0';
    }

    infixToPostfix(infix, postfix);

    printf("Postfix expression: %s\n", postfix);

    int result = evaluatePostfix(postfix);
    printf("Result after evaluation: %d\n", result);

    return 0;
}

