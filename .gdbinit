
cd ./build/src/jql/tests
file ./jql_test1
#set args --ssl

set confirm off
set follow-fork-mode parent
set detach-on-fork on
set print elements 4096

define lb
    set breakpoint pending on
    source ~/.breakpoints
    set breakpoint pending auto
    echo breakpoints loaded\n
end

define sb
    save breakpoints ~/.breakpoints
    echo breakpoints saved\n
end


