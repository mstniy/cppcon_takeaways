# The Hidden Secrets of Move Semantics

[View on Youtube](https://www.youtube.com/watch?v=TFMKjL38xAI)

1. #### `const` disables move semantics
*Timestamp: 5:50, 10:28*

Affects functions returning const values as well as const variables.

Unless you use `const T&&`, but this is a semantic contradiction.

Instead, do not return const values.

2. #### Use `auto&&` in lambdas to have universal references
*Timestamp: 33:55*

To forward, use `std::forward<decltype(x)>(x)`

3. #### Iterating and modifying a `std::vector<bool>` using a for loop
*Timestamp: 37:00*

The naive approach, `for (auto& val : vals)` does not compile since `std::vector<bool>::operator[]` returns a temporary proxy object, and the for loop tries to bind a non-const lvalue to it.

Use `auto&&` instead: `for (auto&& val: vals)`

4. #### Returning references from getters
*Timestamp: 44:07*

Very easy to run into UB:

```c++
class Class {
    ...
    
    vector<int> vals;
    
    const vector<int>& getValues() const {
        return vals;
    }
};

Class getInstance();

for (const auto& v : getInstance().getValues()) // UB
    ...
```
Solution: Have two getters
```c++
class Class {
    ...
    const vector<int>& getValues() const& {
        return vals;
    }
    vector<int> getValues() && { // Called for rvalues
        return std::move(vals);
    }
};
```
