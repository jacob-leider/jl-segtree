#include <vector>

struct Cube
{
  int l;
  int r;

  int Volume() { return r - l; }

  int Center() { return (r + l) / 2; }

  std::pair<Cube, Cube> Subdivide()
  {
    int m = Center();
    return {{l, m}, {m, r}};
  }

  bool IsPoint() { return Volume() == 1; }

  bool IsDisjointFrom(const Cube &other) { return other.l >= r | other.r <= l; }

  bool operator==(Cube const &) const = default;

  Cube operator&(const Cube &other) const { return this->IntersectWith(other); }

  Cube IntersectWith(const Cube &other) const
  {
    int il, ir;
    il = std::max(l, other.l);
    ir = std::min(r, other.r);
    return {il, ir};
  }
};

struct Operation
{
  bool reset_pending = false;
  int to_add = 0;

  int Evaluate(int val, Cube domain)
  {
    if (reset_pending)
      return domain.Volume() * to_add;
    else
      return val + domain.Volume() * to_add;
  }

  void ComposeWith(const Operation &other)
  {
    if (other.reset_pending) // other resets.
      *this = other;
    else
      to_add += other.to_add;
  }

  void Reset()
  {
    // object is zero <=> object is identity.
    memset(this, 0, sizeof(*this));
  }
};

class SegmentTree
{
public:
  SegmentTree(const std::vector<int> &arr)
  {
    size_ = arr.size();
    int tree_size = 4 * size() + 1;
    tree_ = std::vector<int>(tree_size, 0);
    operations_ = std::vector<Operation>(tree_size, Operation());
    // Construct the segment tree.
    BuildTree(arr, 0, arr.size(), 0);
  }

  void ApplyToRange(Cube domain, const Operation &op)
  {
    ApplyOperationR(0, domain, {0, size()}, op);
  }

  void AssignRange(Cube domain, int val) { ApplyToRange(domain, SetOp(val)); }

  void AddToRange(Cube domain, int inc) { ApplyToRange(domain, AddOp(inc)); }

  int QueryRange(Cube domain) { return QueryRangeR(0, domain, {0, size()}); }

  int Get(int i) { return QueryRange({i, i + 1}); }

  int size() { return size_; }

private:
  int size_;

  std::vector<int> tree_;
  std::vector<Operation> operations_;

  Operation AddOp(int add) { return {false, add}; }
  Operation SetOp(int nv) { return {true, nv}; }

  int Left(int v) { return 2 * v + 1; }
  int Right(int v) { return 2 * v + 2; }

  // Recompute based on childrens' values.
  void UpdateValueFromBelow(int v)
  {
    tree_[v] = tree_[Left(v)] + tree_[Right(v)];
  }

  // Recompute based on pending operations.
  void UpdateValueFromAbove(int v, Cube domain)
  {
    tree_[v] = operations_[v].Evaluate(tree_[v], domain);
  }

  // FIXME unclear functionality despite correctness.
  void EvaluateAny(int v, Cube domain, const Operation &op)
  {
    operations_[v].ComposeWith(op);
    UpdateValueFromAbove(v, domain);
    if (domain.IsPoint())
      operations_[v].Reset();
  }

  /**
   * @brief Evaluate the operation for this node, push it onto its
   * children, and reset the operation to the identity.
   */
  void Push(int v, Cube domain)
  {
    auto [left_domain, right_domain] = domain.Subdivide();
    operations_[Left(v)].ComposeWith(operations_[v]);
    operations_[Right(v)].ComposeWith(operations_[v]);
    // Evaluate leaves immediately.
    if (left_domain.IsPoint())
      UpdateValueFromAbove(Left(v), left_domain);
    if (right_domain.IsPoint())
      UpdateValueFromAbove(Right(v), right_domain);
    // Recompute current value and clear the current operation.
    UpdateValueFromAbove(v, domain);
    operations_[v].Reset();
  }

  void ApplyOperationR(int v, Cube query_domain, Cube node_domain,
                       const Operation &op)
  {
    // Assume node_domain contains query_domain
    auto [left_node_domain, right_node_domain] = node_domain.Subdivide();

    if (query_domain == node_domain) // range covers this node.
      EvaluateAny(v, query_domain, op);
    else
    {
      Push(v, node_domain);

      // Apply to left subset.
      if (!query_domain.IsDisjointFrom(left_node_domain))
        ApplyOperationR(Left(v), left_node_domain.IntersectWith(query_domain),
                        left_node_domain, op);
      // Apply to right subset.
      if (!query_domain.IsDisjointFrom(right_node_domain))
        ApplyOperationR(Right(v), right_node_domain.IntersectWith(query_domain),
                        right_node_domain, op);

      UpdateValueFromBelow(v);
    }
  }

  int QueryRangeR(int v, Cube query_domain, Cube node_domain)
  {
    // Assume node_domain contains query_domain
    auto [left_node_domain, right_node_domain] = node_domain.Subdivide();

    if (query_domain == node_domain) // range covers this node.
      return tree_[v];
    else
    {
      Push(v, node_domain); // Defer overwrites.

      int sum = 0;
      // Query left subset.
      if (!query_domain.IsDisjointFrom(left_node_domain))
        sum +=
            QueryRangeR(Left(v), left_node_domain.IntersectWith(query_domain),
                        left_node_domain);
      // Query right subset.
      if (!query_domain.IsDisjointFrom(right_node_domain))
        sum +=
            QueryRangeR(Right(v), right_node_domain.IntersectWith(query_domain),
                        right_node_domain);
      return sum;
    }
  }

  void BuildTree(const std::vector<int> &arr, int l, int r, int v)
  {
    if (r - l == 1)
      tree_[v] = arr[l];
    else
    {
      BuildTree(arr, l, (l + r) / 2, Left(v));
      BuildTree(arr, (l + r) / 2, r, Right(v));
      UpdateValueFromBelow(v);
    }
  }
};
