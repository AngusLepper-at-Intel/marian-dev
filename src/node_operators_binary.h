#pragma once

#include <thread>

#include "node.h"
#include "thrust_functions.h"
#include "tensor_operators.h"

namespace marian {

struct BinaryNodeOp : public Node {
  Expr a_;
  Expr b_;

  template <typename ...Args>
  BinaryNodeOp(Expr a, Expr b, Args ...args)
   : Node(a->graph(),
      keywords::shape=keywords::Get(keywords::shape, a->shape(), args...),
      keywords::no_inference=a->skipped_inference()
      || b->skipped_inference()
      || keywords::Get(keywords::no_inference, false, args...),
          keywords::no_training=a->skipped_training()
      || b->skipped_training()
      || keywords::Get(keywords::no_training, false, args...),
      args...), a_(a), b_(b)
  {
  remove_children_from_top_nodes();
  }

  ~BinaryNodeOp() {}

  void remove_children_from_top_nodes();

  void backward_debug(Float delta) {
    //using namespace std;
    //
    //cerr << "BinaryNodeOp::" << typeid(*this).name() << "::backward_debug()" << endl;
    //
    //std::vector<float> preCalcGradA, diffGradA, numericalGradA;
    //preCalcGradA << a_->grad();
    ////output("preCalcGradA", preCalcGradA);
    //
    //std::vector<float> preCalcGradB, diffGradB, numericalGradB;
    //preCalcGradB << b_->grad();
    ////output("preCalcGradB", preCalcGradB);
    //
    //// use df/dx to calc grad
    //backward();
    //cerr << "orig a_->grad()=" << a_->grad().Debug() << endl;
    //cerr << "orig b_->grad()=" << b_->grad().Debug() << endl;
    //
    //diffGradA << a_->grad();
    //diffGradB << b_->grad();
    //
    ////cerr << "orig a_->grad()=" << a_->grad().Debug() << endl;
    ////cerr << "orig b_->grad()=" << b_->grad().Debug() << endl;
    //
    //cerr << "TENSOR A:" << endl;
    //a_->grad().set(preCalcGradA);
    //b_->grad().set(preCalcGradB);
    //
    //calc_numeric_grad(delta, a_->val(), a_->grad());
    //cerr << "numerical a_->grad()=" << a_->grad().Debug() << endl;
    //
    //numericalGradA << a_->grad();
    //outputL2Norm("TENSOR A", diffGradA, numericalGradA);
    //
    //
    //cerr << "TENSOR B:" << endl;
    //a_->grad().set(preCalcGradA);
    //b_->grad().set(preCalcGradB);
    //
    //calc_numeric_grad(delta, b_->val(), b_->grad());
    //cerr << "numerical b_->grad()=" << b_->grad().Debug() << endl;
    //
    //numericalGradB << b_->grad();
    //outputL2Norm("TENSOR B", diffGradB, numericalGradB);
    //
    //// reset to diff grad
    //a_->grad().set(diffGradA);
    //b_->grad().set(diffGradB);
  }


};

/**
 * @brief Represents a node in an expression graph capable of performing
 *        <a href="https://en.wikipedia.org/wiki/Matrix_multiplication#Matrix_product_.28two_matrices.29">matrix
 *        multiplication</a> of two input matrices.
 */
struct DotNodeOp : public BinaryNodeOp {
  template <typename ...Args>
  DotNodeOp(Expr a, Expr b, Args ...args)
  : BinaryNodeOp(a, b,
                 keywords::shape=newShape(a, b),
                 args...) { }

  Shape newShape(Expr a, Expr b) {
    Shape shape1 = a->shape();
    Shape shape2 = b->shape();
    UTIL_THROW_IF2(shape1[1] != shape2[0],
                   "matrix product requires dimensions to match");
    shape1.set(1, shape2[1]);
    return shape1;
  }

  void forward() {
    // C = A*B
    Prod(val_, a_->val(), b_->val(), false, false);
  }

  void backward() {
    // D is the adjoint, the matrix of derivatives
    // df/dA += D*B.T
    // df/dB += A.T*D
    // beta set to 1.0 in gemm, C = dot(A,B) + beta * C
    // to sum gradients from different graph parts

    Prod(a_->grad(), adj_, b_->val(), false, true, 1.0);
    Prod(b_->grad(), a_->val(), adj_, true, false, 1.0);
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=" << label("•")
      << ", style=\"filled\", fillcolor=\"orange\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl;
    ss << "\"" << b_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct ScalarProductNodeOp : public BinaryNodeOp {
  template <typename ...Args>
  ScalarProductNodeOp(Expr a, Expr b, Args ...args)
  : BinaryNodeOp(a, b,
                 keywords::shape=newShape(a, b, args...),
                 args...) { }

  template <typename ...Args>
  Shape newShape(Expr a, Expr b, Args ...args) {
    int ax = keywords::Get(keywords::axis, -1, args...);
    Shape full = a->shape();
    for(int i = 0; i < b->shape().size(); ++i)
      full.set(i, std::max(full[i], b->shape()[i]));

    if(ax != -1) {
      full.set(ax, 1);
    }
    else {
      full.set(0, 1);
      full.set(1, 1);
      full.set(2, 1);
      full.set(3, 1);
    }
    return full;
  }

  void forward() {
    Reduce(_1 * _2,
           val_, a_->val(), b_->val());
  }

  void backward() {
    // @TODO: check gradient
    Add(_1 * _2,
        a_->grad(), b_->val(), adj_);
    Add(_1 * _2,
        b_->grad(), a_->val(), adj_);
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=" << label("scalar-product")
      << ", style=\"filled\", fillcolor=\"orange\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl;
    ss << "\"" << b_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};


struct ElementBinaryNodeOp : public BinaryNodeOp {
  template <typename ...Args>
  ElementBinaryNodeOp(Expr a, Expr b, Args ...args)
   : BinaryNodeOp(a, b,
                  keywords::shape=newShape(a, b),
                  args...) {}

  Shape newShape(Expr a, Expr b) {
    Shape shape1 = a->shape();
    Shape shape2 = b->shape();
    for(int i = 0; i < shape1.size(); ++i) {
      UTIL_THROW_IF2(shape1[i] != shape2[i] && shape1[i] != 1 && shape2[i] != 1,
                     "Shapes cannot be broadcasted");
      shape1.set(i, std::max(shape1[i], shape2[i]));
    }
    return shape1;
  }

};

struct PlusNodeOp : public ElementBinaryNodeOp {
  template <typename ...Args>
  PlusNodeOp(Args ...args)
    : ElementBinaryNodeOp(args...) { }

  void forward() {
    Element(_1 = _2 + _3,
            val_, a_->val(), b_->val());
  }

  void backward() {
    Add(_1, a_->grad(), adj_);
    Add(_1, b_->grad(), adj_);
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=" << label("+")
      << ", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl;
    ss << "\"" << b_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct MinusNodeOp : public ElementBinaryNodeOp {
  template <typename ...Args>
  MinusNodeOp(Args ...args)
    : ElementBinaryNodeOp(args...) { }

  void forward() {
    Element(_1 = _2 - _3,
            val_, a_->val(), b_->val());
  }

  void backward() {
    Add( _1, a_->grad(), adj_);
    Add(-_1, b_->grad(), adj_);
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=" << label("-")
      << ", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl;
    ss << "\"" << b_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct MultNodeOp : public ElementBinaryNodeOp {
  template <typename ...Args>
  MultNodeOp(Args ...args)
    : ElementBinaryNodeOp(args...) { }

  void forward() {
    Element(_1 = _2 * _3,
            val_, a_->val(), b_->val());
  }

  void backward() {
    Add(_1 * _2,
        a_->grad(), adj_, b_->val());
    Add(_1 * _2,
        b_->grad(), adj_, a_->val());
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=" << label("×")
      << ", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl;
    ss << "\"" << b_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

struct DivNodeOp : public ElementBinaryNodeOp {
  template <typename ...Args>
  DivNodeOp(Args ...args)
    : ElementBinaryNodeOp(args...) { }

  void forward() {
    Element(_1 = _2 / _3,
            val_, a_->val(), b_->val());
  }

  void backward() {
    Add(_1 * 1.0f / _2,
        a_->grad(), adj_, b_->val());
    Add(-_1 * _2 / (_3 * _3),
        b_->grad(), adj_, a_->val(), b_->val());
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=" << label("÷")
      << ", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl;
    ss << "\"" << b_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };

};

// Cross-entropy node. It computes -b*log(softmax(a)), summing rowwise.
struct CrossEntropyNodeOp : public BinaryNodeOp {
  template <typename ...Args>
    CrossEntropyNodeOp(Expr a, Expr b, Args ...args)
    : BinaryNodeOp(a, b,
                   keywords::shape=newShape(a),
                   args...) { }

  Shape newShape(Expr a) {
    Shape shape1 = a->shape();
    shape1.set(1, 1);
    return shape1;
  }

  void forward() {
    // C = sum(-logsoftmax(A) * delta(y', y))
    CrossEntropyPick(val_, a_->val(), b_->val());
  }


  void backward() {
    // @TODO: save memory for the second derivative.
    // Caching is not required, recomputation saves a lot of memory while not
    // being slower.
    CrossEntropyPickBackward(a_->grad(), adj_, a_->val(), b_->val());
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=" << label("x-ent")
      << ", style=\"filled\", fillcolor=\"orange\"]" << std::endl;
    ss << "\"" << a_ << "\" -> \"" << this << "\"" << std::endl;
    ss << "\"" << b_ << "\" -> \"" << this << "\"" << std::endl << std::endl;
    return ss.str();
  };
};

// an n-ary node

struct NaryNodeOp : public Node {
  std::vector<Expr> children_;

  template <typename ...Args>
  NaryNodeOp(const std::vector<Expr>& nodes, Args ...args)
   : Node(nodes.back()->graph(),
      keywords::shape=keywords::Get(keywords::shape, nodes.back()->shape(), args...),
      keywords::no_inference=
      std::any_of(nodes.begin(), nodes.end(),
                  [](Expr a) { return a->skipped_inference(); })
      || keywords::Get(keywords::no_inference, false, args...),
      keywords::no_training=
      std::any_of(nodes.begin(), nodes.end(),
                  [](Expr a) { return a->skipped_training(); })
      || keywords::Get(keywords::no_training, false, args...),
      args...), children_(nodes)
  {
    remove_children_from_top_nodes();
  }

  ~NaryNodeOp() {}

  void remove_children_from_top_nodes();

  void backward_debug(Float delta) {}

};

struct ConcatenateNodeOp : public NaryNodeOp {
  template <typename ...Args>
  ConcatenateNodeOp(const std::vector<Expr>& nodes, Args ...args)
    : NaryNodeOp(nodes,
                 keywords::shape=newShape(nodes, keywords::Get(keywords::axis, 0, args...)),
                 args...), ax_(keywords::Get(keywords::axis, 0, args...)) { }

  Shape newShape(const std::vector<Expr>& nodes, int ax) {
    Shape shape = nodes.back()->shape();
    shape.set(ax, 0);
    for(auto child : nodes)
      shape.set(ax, shape[ax] + child->shape()[ax]);
    //std::cerr << ax << " : " << shape[0] << " " << shape[1] << std::endl;
    return shape;
  }

  /*
  void allocate() {
    graph()->Tensor(val_, shape);
    placeValue(val_->data());
  }

  void placeValue(float* data) {
    val_.reset(data);
    for(auto child : children_) {
      child->deallocateValue();
      child->placeValue(data);
      child->val()->set(0);
      data += child->val()->size();
    }
  }

  void forward() {}
  void backward() {}

  void init_dependent() {}
  void set_zero_adjoint() {}
 
  */


  void forward() {
    std::vector<Tensor> concatenees;
    for(auto child : children_)
      concatenees.push_back(child->val());
    Concatenate(val_, concatenees, ax_);
  }

  void backward() {
    std::vector<Tensor> deconcatenees;
    for(auto child : children_)
      deconcatenees.push_back(child->grad());
    Deconcatenate(deconcatenees, adj_, ax_);
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=" << label("concat")
      << ", style=\"filled\", fillcolor=\"orange\"]" << std::endl;
    for(auto child : children_)
      ss << "\"" << child << "\" -> \"" << this << "\"" << std::endl;
    ss << std::endl;
    return ss.str();
  };

  int ax_;
};

struct TanhPlus3NodeOp : public NaryNodeOp {
  TanhPlus3NodeOp(const std::vector<Expr>& nodes)
    : NaryNodeOp(nodes, keywords::shape=newShape(nodes)) { }

  Shape newShape(const std::vector<Expr>& nodes) {
    Shape shape = nodes[0]->shape();

    for(int n = 1; n < nodes.size(); ++n) {
      Shape shapen = nodes[n]->shape();
      for(int i = 0; i < shapen.size(); ++i) {
        UTIL_THROW_IF2(shape[i] != shapen[i] && shape[i] != 1 && shapen[i] != 1,
                       "Shapes cannot be broadcasted");
        shape.set(i, std::max(shape[i], shapen[i]));
      }
    }
    return shape;
  }

  void forward() {
    Element(_1 = Tanh(_2 + _3 + _4),
            val_,
            children_[0]->val(),
            children_[1]->val(),
            children_[2]->val());
  }

  void backward() {
    Add((1.f - _1 * _1) * _2,
        children_[0]->grad(), val_, adj_);
    Add((1.f - _1 * _1) * _2,
        children_[1]->grad(), val_, adj_);
    Add((1.f - _1 * _1) * _2,
        children_[2]->grad(), val_, adj_);
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=" << label("tanhPlus3")
      << ", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    for(auto child : children_)
      ss << "\"" << child << "\" -> \"" << this << "\"" << std::endl;
    ss << std::endl;
    return ss.str();
  }
};

struct AffineNodeOp : public NaryNodeOp {
  AffineNodeOp(const std::vector<Expr>& nodes)
    : NaryNodeOp(nodes, keywords::shape=newShape(nodes)) { }

  Shape newShape(const std::vector<Expr>& nodes) {
    Shape shape1 = nodes[0]->shape();
    Shape shape2 = nodes[1]->shape();
    UTIL_THROW_IF2(shape1[1] != shape2[0],
                   "matrix product requires dimensions to match");
    shape1.set(1, shape2[1]);
    return shape1;
  }

  void forward() {
    Prod(val_, children_[0]->val(), children_[1]->val(), false, false);
    Add(_1, val_, children_[2]->val());
  }

  void backward() {
    // D is the adjoint, the matrix of derivatives
    // df/dA += D*B.T
    // df/dB += A.T*D
    // beta set to 1.0 in gemm, C = dot(A,B) + beta * C
    // to sum gradients from different graph parts

    Prod(children_[0]->grad(), adj_, children_[1]->val(), false, true, 1.0);
    Prod(children_[1]->grad(), children_[0]->val(), adj_, true, false, 1.0);
    Add(_1, children_[2]->grad(), adj_);
  }

  virtual std::string graphviz() {
    std::stringstream ss;
    ss << "\"" << this << "\" [shape=\"box\", label=" << label("tanhPlus3")
      << ", style=\"filled\", fillcolor=\"yellow\"]" << std::endl;
    for(auto child : children_)
      ss << "\"" << child << "\" -> \"" << this << "\"" << std::endl;
    ss << std::endl;
    return ss.str();
  }
};


}
