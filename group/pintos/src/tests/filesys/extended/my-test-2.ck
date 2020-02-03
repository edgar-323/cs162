# -*- perl -*-
use strict;
use warnings;
use tests::tests;
use tests::random;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(my-test-2) begin
(my-test-2) create "a"
(my-test-2) open "a"
(my-test-2) 100 kB written to "a"
(my-test-2) 0 block_read calls in 200 writes
(my-test-2) 200 block_write calls in 200 writes
(my-test-2) close "a"
(my-test-2) end
EOF
pass;
