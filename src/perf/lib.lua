function exec_lua(worker)
    local math = require("math")
    local x = worker.a_ * worker:f()
    local y = worker.b_ * worker:g()
    local z = worker.c_ * worker:h()
    if math.abs(x) < math.abs(y) then
        return (math.pow(x, 2) + math.pow(y, 2)) * z
    else
        return (math.pow(x, 2) - math.pow(y, 2)) * z
    end
end
