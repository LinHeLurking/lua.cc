-- Multi-argument function with intended error.
function err_print_all(a, b, c)
    print(a, b, c)
    -- Error: indexing a nil
    print(X[0])
end

function print_all(a, b, c)
    print(a, b, c)
end
