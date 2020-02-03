# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(sc-null) begin
sc-null: exit(-1)
EOF
pass;
