def fib(x)
  #recursive call function
  if x < 3 then
    x + 1
  else
    fib(x-1)+fib(x-2)
