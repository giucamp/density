

density explained in the real world: the function queue
---------------------------

Imagine that you have to write down a list of things you have to do. You choose to use a lined paper. Let's begin:

![enter image description here](http://peggysansonetti.it/tech/density/todolist/first.png)

You have a row for each point in your list, and this is cool.

Anyway, going on, you discover that sometimes the text is too long, and it doesn't fit to a row. In these cases you have to paste on your sheet another piece of paper, and write on the latter. This is annoying, since you have to find a piece large enough, which is time consuming.
Some other times the text is very short, so you are wasting space:

![enter image description here](http://peggysansonetti.it/tech/density/todolist/second.png)

Furthermore, when you read back the list, you have to switch between papers, which again is time consuming.

Then you realize that is better to use a blank paper, and to make the lines by yourself:

![enter image description here](http://peggysansonetti.it/tech/density/todolist/dense.png)

Now let's go to C++:

 - The lined paper is a `std::queue<std::function>. Every function has
   a fixed-size inplace storage, that may be too small or too big. This is bad in both cases.
   
 - The paper is memory. When the functor does not fit in the inplace storage of the std::function,
   another block of memory is allocated on the heap. Actually in C++
   this is worse than in the real world: you may think that in the lined paper, instead of pasting the small green piece, you write where it is. When you read back, you have to find the paper again.
   
 - density provides [function_queue](http://peggysansonetti.it/tech/density/html/classdensity_1_1function__queue_3_01RET__VAL_07PARAMS_8_8_8_08_4.html), which, for each functor, reserves only
   the actual needed size (the length of your text). A compact memory layout means [much better performances](http://peggysansonetti.it/tech/density/html/func_queue_bench.html).

> Written with [StackEdit](https://stackedit.io/).