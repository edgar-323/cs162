// vim:nocindent:tabstop=2:expandtab
// TWO SPACES INSTEAD OF TABS!

/* Struct definition */
struct <NAME>
  {
    int member;
    enum member;
    /* ... */
  };

/* Enum definition */
enum <NAME>
  {
    NAME,
    NAME,
    NAME,
    Name      // No final comma
  };

/* Function definition */
<return type>
<name> (void) // (void) instead of blank parens, and space between name and args
{
  /* Code */
}

/* Function calling */
<function> (args);    // Space between name and args.

/* If-else */

/* For single-line if statements, don't use brackets. */
if (<cond>)
  <action>
else if (<cond2>)
  <action2>
else
  <action3>

/* Otherwise, standard syntax. */
if (<cond) {
  <action>
  <action>
  <action>
}

/* For loops */
for (<init>; <cond>; <pred>)
  {
    <action>
    <action>
    <action>
  }

/* For single-line for loops, don't use brackets. */
for (<init>; <cond>; <pred>)
  <action>

/* While loops */
while (<cond>)
  {
    <action>
    <action>
    <action>
  }

/* For single-line while loops, don't use brackets. */
while (<cond>)
  <action>

/* Switch statements. Note that cases aren't double indented. */
switch (<expr>)
  {
  case <expr>:
    <action>
  case <expr>:
    <action>
  default:
    <action>
  }

/* For NULL checks, actually check against NULL. */
/* Also, nested functions have spaces before their args,
 *  but none at the end. */
int *x = malloc (sizeof (int)); // Asterisk goes on the identifier, not the type.
if (x == NULL)
  PANIC

/* Arithmetic */
int x = a + b * c;  // Spaces inbetween operators and operands.

/* Argument format */
<func> (<arg1>, <arg2>, <arg3>);  // Spaces after commas, but not before.

/* Array indexing */
<array>[<index>]; // No space before the braces.

/* No newline at the end of file. */
