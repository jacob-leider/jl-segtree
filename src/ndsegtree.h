#include <array>
#include <string>
#include <vector>

template <int n> struct Cube
{
  static constexpr int N = 1 << n;
  std::array<int, n> l;
  std::array<int, n> r;

  int Volume() const
  {
    int prod = 1;
    for (int i = 0; i < n; i++)
      prod *= r[i] - l[i];
    return prod;
  }

  std::array<int, n> Center() const
  {
    std::array<int, n> center;
    for (int i = 0; i < n; i++)
      center[i] = (l[i] + r[i]) / 2;
    return center;
  }

  std::array<Cube<n>, N> Subdivide() const
  {
    std::array<int, n> m = Center();
    std::array<Cube<n>, N> quadrants;
    // The bits of i determine the (generalized to 2^n) quadrant. 0 for low, 1
    // for high.
    //
    // For example, let i = 2, l = [-3, 0], r = [4, 2], center = [0,
    // 1]. In binary, i = 10. Since the first bit is 1 and the second 0, we get
    // the quadrant [0, 4] x [0, 1].
    for (int i = 0; i < N; i++)
    {
      Cube quadrant;
      // j is the bit, k is the index.
      for (int k = 0; k < n; k++)
      {
        if (i & (1 << k))
        {
          quadrant.l[k] = l[k];
          quadrant.r[k] = m[k];
        }
        else
        {
          quadrant.l[k] = m[k];
          quadrant.r[k] = r[k];
        }
      }
      quadrants[i] = quadrant;
    }

    return quadrants;
  }

  bool IsEmpty() const { return Volume() == 0; }

  bool IsPoint() const { return Volume() == 1; }

  bool IsDisjointFrom(const Cube<n> &other) const
  {
    // They only intersect if every interval intersects.
    for (int i = 0; i < n; i++)
      if (other.l >= r | other.r <= l)
        return true;

    return false;
  }

  bool operator==(Cube<n> const &other) const
  {
    for (int i = 0; i < n; i++)
      if (l[i] != other.l[i] | r[i] != other.r[i])
        return false;

    return true;
  }

  Cube<n> operator&(const Cube<n> &other) const
  {
    return this->IntersectWith(other);
  }

  Cube<n> IntersectWith(const Cube<n> &other) const
  {
    Cube<n> intersection;
    for (int i = 0; i < n; i++)
    {
      int lo = std::max(l[i], other.l[i]);
      int hi = std::min(r[i], other.r[i]);
      if (lo >= hi)
      {
        // FIXME Find a better approach to this.
        // Volume will be zero anyway...
        intersection.l[i] = 0;
        intersection.r[i] = 0;
      }
      else
      {
        intersection.l[i] = lo;
        intersection.r[i] = hi;
      }
    }

    return intersection;
  }
};

struct Operation
{
  bool reset_pending = false;
  int to_add = 0;

  template <int n> int Evaluate(int val, Cube<n> domain)
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

template <int n> class SegmentTree
{
public:
  static constexpr int N = 1 << n;

  SegmentTree(const std::vector<int> &arr, const std::array<int, n> &dims)
  {
    entire_domain_ = {std::array<int, n>(), dims};
    int tree_size =
        4 * entire_domain_.Volume(); // FIXME derive a tighter bound.
    tree_ = std::vector<int>(tree_size, 0);
    operations_ = std::vector<Operation>(tree_size, Operation());
    // Construct the segment tree.
    BuildTree(arr);
  }

  void ApplyToRange(Cube<n> domain, const Operation &op)
  {
    ApplyOperationR(0, domain, entire_domain_, op);
  }

  void AssignRange(Cube<n> domain, int val)
  {
    ApplyToRange(domain, {true, val});
  }

  void AddToRange(Cube<n> domain, int inc)
  {
    ApplyToRange(domain, {false, inc});
  }

  int QueryRange(Cube<n> domain)
  {
    return QueryRangeR(0, domain, entire_domain_);
  }

  int Get(std::array<int, n> I)
  {
    Cube<n> domain;
    domain.l = I;
    for (int i = 0; i < n; i++)
      domain.r[i] = I[i] + 1;

    return QueryRange(domain);
  }

  const std::array<int, n> dims() const { return entire_domain_.r; }

private:
  Cube<n> entire_domain_;
  std::vector<int> tree_;
  std::vector<Operation> operations_;

  // i = index of child.
  int Child(int v, int i)
  {
    // N-ary tree.
    return N * v + i + 1;
  }

  // Recompute based on childrens' values.
  void UpdateValueFromBelow(int v)
  {
    int sum = 0;
    for (int i = 0; i < N; i++)
      sum += tree_[Child(v, i)];
    tree_[v] = sum;
  }

  // Recompute based on pending operations.
  void UpdateValueFromAbove(int v, Cube<n> domain)
  {
    tree_[v] = operations_[v].template Evaluate<n>(tree_[v], domain);
  }

  // FIXME unclear functionality despite correctness.
  void EvaluateAny(int v, Cube<n> domain, const Operation &op) // FIXME Rename.
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
  void Push(int v, const Cube<n> &domain, const std::array<Cube<n>, N> &quads)
  {
    for (int i = 0; i < N; i++)
    {
      const Cube<n> quad = quads[i];
      operations_[Child(v, i)].ComposeWith(operations_[v]);

      // Evaluate leaves immediately.
      if (quad.IsPoint())
      {
        UpdateValueFromAbove(Child(v, i), quad);
        operations_[Child(v, i)].Reset();
      }
    }

    // Recompute current value and clear the current operation.
    UpdateValueFromAbove(v, domain);
    operations_[v].Reset();
  }

  void ApplyOperationR(int v, Cube<n> query_domain, Cube<n> domain,
                       const Operation &op)
  {
    // Assume node_domain contains query_domain
    if (query_domain == domain) // range covers this node.
      EvaluateAny(v, query_domain, op);
    else
    {
      const std::array<Cube<n>, N> quads = domain.Subdivide();

      Push(v, domain, quads); // Defer current operation.

      for (int i = 0; i < N; i++)
      {
        const Cube<n> quad = quads[i];
        const Cube<n> query_sub = quad.IntersectWith(query_domain);

        // FIXME Rendundant recomputation of the intersection.
        if (!query_sub.IsEmpty())
          ApplyOperationR(Child(v, i), query_sub, quad, op);
      }

      UpdateValueFromBelow(v);
    }
  }

  int QueryRangeR(int v, Cube<n> query_domain, Cube<n> domain)
  {
    // Assume node_domain contains query_domain
    if (query_domain == domain) // range covers this node.
      return tree_[v];
    else
    {
      std::array<Cube<n>, N> quads = domain.Subdivide();

      Push(v, domain, quads); // Defer overwrites.

      int sum = 0;
      for (int i = 0; i < N; i++)
      {
        Cube<n> quad = quads[i];
        Cube<n> query_sub = quad.IntersectWith(query_domain);

        if (!query_sub.IsEmpty())
          sum += QueryRangeR(Child(v, i), query_sub, quad);
      }

      return sum;
    }
  }

  void BuildTree(const std::vector<int> &arr)
  {
    // Build entire domain.
    Cube<n> domain = {std::array<int, n>(), dims()};
    BuildTreeR(arr, domain, 0);
  }

  int Linear(const std::array<int, n> &coords) const
  {
    int idx = coords.front();
    for (int i = 0; i < n - 1; i++)
      idx = dims()[i] * idx + coords[i + 1];
    return idx;
  }

  void BuildTreeR(const std::vector<int> &arr, Cube<n> domain, int v)
  {
    if (domain.IsPoint())
      tree_[v] = arr[Linear(domain.l)];
    else
    {
      std::array<Cube<n>, N> quads = domain.Subdivide();
      for (int i = 0; i < N; i++)
        BuildTreeR(arr, quads[i], Child(v, i));

      UpdateValueFromBelow(v);
    }
  }
};
