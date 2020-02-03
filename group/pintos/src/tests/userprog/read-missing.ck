# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF', <<'EOF']);
(read-missing) begin
(read-missing) open "sample.txt"
(child-remove) begin
(child-remove) end
child-remove: exit(0)
(read-missing) wait(exec()) = 0
(read-missing) verified contents of "sample.txt"
(read-missing) end
read-missing: exit(0)
EOF
(read-missing) begin
(read-missing) open "sample.txt"
(child-remove) begin
child-remove: exit(-1)
(read-missing) wait(exec()) = -1
(read-missing) verified contents of "sample.txt"
(read-missing) end
read-missing: exit(0)
EOF
pass;
