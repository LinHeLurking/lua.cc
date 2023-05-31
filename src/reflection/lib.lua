function greet_person(name, age, career)
  print("[Lua] Hello! I'm " .. name .. " and I'm " .. age .. " years old!")
  print("[Lua] I'm a " .. career .. "!")
end

function a_plus_b(a, b)
  print("[Lua] calculate " .. a .. " + " .. b)
  local result = a + b
  print("[Lua] result is " .. result)
  return result
end

function num_n_str()
  return -1, "Hello!"
end

function describe_obj(obj)
  print(getmetatable(obj))
  for k,v in pairs(getmetatable(obj)) do
    print(k, v)
  end
  -- print("[Lua] This is obj: {" .. "x_: " .. obj.x_ .. "y_: " .. obj.y_ .. "}")
  -- print("[Lua] Change value inside lua")
  -- obj.x_ = -1
  -- obj.y_ = "Lua Surprise!"
end
