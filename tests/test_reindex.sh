#!/bin/bash
perl -pi -e 's/[0-9]+\/\*offset\*\//++$i."\/\*offset\*\/"/ge' $1