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

function consume_obj_a(obj)
  print("[Lua] This is obj: {" .. "x_: " .. obj.x_ .. ", y_: " .. obj.y_ .. "}")
  print("[Lua] Change value inside lua")
  obj.x_ = -1
  obj.y_ = "Lua Surprise!"
end

function consume_obj_b(obj)
  print("[Lua] This is obj: {" .. "u_: " .. obj.u_ .. ", v_: " .. obj.v_ .. "}")
  print("[Lua] Change value inside lua")
  obj.u_ = "Lua Surprise!"
  obj.v_ = -1
end

function take_action(obj)
  print("[Lua] These are results on different actions: ")
  local act_list = { "add", "sub", "mul", "div" }
  local a, b = 3, 4
  for _, act in ipairs(act_list) do
    print("", act .. "(" .. a .. ", " .. b .. "):", obj:take_action(act, a, b))
  end
end

function overwrite_append(x, y)
  x:overwrite_append(y, " [[ Modified in Lua! ]] ")
end

lib = {}

function lib.a_and_b(a, b)
  print("[Lua] Calculate a and b.")
  if a and b then
    print("[Lua] Returning true")
  else
    print("[Lua] Returning false")
  end
  return a and b
end
