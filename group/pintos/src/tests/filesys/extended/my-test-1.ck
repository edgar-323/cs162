# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(my-test-1) begin
(my-test-1) create "a"
(my-test-1) open "a"
(my-test-1) close "a"
(my-test-1) reset-buffer
(my-test-1) open "a"
(my-test-1) read "a"
(my-test-1) close "a"
(my-test-1) open "a"
(my-test-1) read "a"
(my-test-1) close "a"
(my-test-1) Cache hit rate improved.
(my-test-1) end
EOF
pass;
