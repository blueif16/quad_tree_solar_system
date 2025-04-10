To install libraries, use following commands:

sudo apt update
sudo apt-get install libsdl2-dev
sudo apt-get install libsdl2-ttf-dev


Can follow sample tasks to compile

Some gpt generated guidence for how to involve the quad tree and calculations

To address your query about enhancing your solar system simulation by implementing the quad tree and Barnes-Hut algorithm for more accurate force calculations, including gravitational interactions between all bodies (not just the Sun), and monitoring the movements of added asteroids, I’ll explain why the quad tree is beneficial and provide a detailed plan for implementation.

### Why the Quad Tree with Barnes-Hut Helps

Your current simulation calculates gravitational forces on planets considering only the Sun’s mass, ignoring interactions between planets and other bodies like asteroids. While the Sun’s gravitational pull dominates, interactions between planets and smaller bodies can influence orbits, especially over time or during close encounters. To account for all pairwise gravitational interactions directly, you’d need to compute forces between every pair of bodies. For \( N \) bodies (planets plus asteroids), this requires \( O(N^2) \) calculations per time step, which becomes impractical as \( N \) increases—say, when you add many asteroids.

The **Barnes-Hut algorithm**, paired with a **quad tree**, reduces this complexity to \( O(N \log N) \), making it efficient for larger systems. Here’s why:

- **Spatial Grouping**: A quad tree divides the 2D simulation space into four quadrants recursively, organizing bodies (planets and asteroids) hierarchically based on their positions. Each node in the tree represents a region and stores the total mass and center of mass of bodies within it.
- **Approximation**: For a given body, the algorithm approximates the gravitational force from distant groups of bodies as if they were a single mass at their center of mass, rather than calculating forces from each body individually. This reduces the number of calculations significantly.
- **Efficiency**: The quad tree enables quick identification of which bodies or groups are “far enough” to approximate, controlled by a parameter called the opening angle (\( \theta \)). This balance between accuracy and speed is ideal for a simulation with many asteroids.

By implementing this, your simulation will:
- Accurately model interactions between all bodies (Sun, planets, asteroids).
- Scale efficiently as you add more asteroids.
- Allow you to monitor how gravitational forces affect asteroid movements realistically.

### Detailed Plan to Proceed

Below is a step-by-step plan to implement the quad tree and Barnes-Hut algorithm in your simulation, integrating asteroid handling and monitoring:

#### 1. Define the Quad Tree Data Structure
- **Purpose**: Create a structure to organize bodies spatially.
- **Implementation**:
  - Each quad tree node should store:
    - **Bounding box**: Coordinates (x, y) and dimensions (width, height) of the region.
    - **Total mass**: Sum of masses of bodies in the node’s region.
    - **Center of mass**: Weighted average position of bodies in the node.
    - **Children**: Pointers to four child nodes (null if a leaf).
    - **Body**: For leaf nodes, the single body it contains (null if internal).
  - A node is a leaf if it contains one or zero bodies; otherwise, it’s internal with four children (northwest, northeast, southwest, southeast).

#### 2. Build the Quad Tree
- **Purpose**: Insert all bodies (planets and asteroids) into the quad tree each time step.
- **Steps**:
  - Start with a root node covering the entire simulation space (e.g., a square large enough to contain all bodies).
  - For each body:
    - If the current node is empty (a leaf with no body), place the body there.
    - If the node has a body (leaf with one body):
      - Subdivide into four child nodes.
      - Redistribute the existing body and the new body into the appropriate child based on their (x, y) positions.
    - If the node is internal, recurse into the child quadrant containing the body’s position.
  - **Note**: Rebuild the quad tree each time step since bodies move.

#### 3. Calculate Centers of Mass
- **Purpose**: Prepare each node for force approximations.
- **Steps**:
  - Traverse the quad tree bottom-up (post-order traversal):
    - For leaf nodes: Set total mass to the body’s mass and center of mass to its position.
    - For internal nodes:
      - Total mass = sum of child nodes’ total masses.
      - Center of mass = weighted average of child nodes’ centers of mass (weight = mass of each child).
  - This step ensures every node has the data needed for Barnes-Hut approximations.

#### 4. Implement Barnes-Hut Force Calculation
- **Purpose**: Compute the total gravitational force on each body efficiently.
- **Steps**:
  - For each body (planet or asteroid):
    - Start at the quad tree root and call a recursive function `calculateForce(body, node)`:
      - If the node is a leaf:
        - If it contains a different body, compute the gravitational force directly using \( F = G \cdot \frac{m_1 m_2}{r^2} \) (skip if it’s the same body to avoid self-interaction).
      - If the node is internal:
        - Compute \( d \): distance from the body to the node’s center of mass.
        - Compute \( s \): size of the node (e.g., width of the bounding box).
        - If \( s / d < \theta \) (e.g., \( \theta = 0.5 \)):
          - Approximate the force using the node’s total mass and center of mass.
        - Else, recurse into each child node and sum their contributions.
    - Sum all force contributions to get the total force on the body.
  - **Parameter**: Tune \( \theta \) (0.5–1.0) for accuracy vs. speed trade-off.

#### 5. Update Simulation Dynamics
- **Purpose**: Apply forces to update body positions and velocities.
- **Steps**:
  - For each body:
    - Acceleration: \( a = F / m \).
    - Update velocity and position using a numerical integrator (e.g., Euler: \( v = v + a \cdot dt \), \( x = x + v \cdot dt \); or preferably Verlet for better accuracy).
  - Time step \( dt \) should be small enough for stability.

#### 6. Add and Handle Asteroids
- **Purpose**: Include asteroids in the simulation.
- **Steps**:
  - Define asteroids with:
    - Mass (smaller than planets).
    - Initial position and velocity (e.g., randomly within the simulation space or near an asteroid belt).
  - Treat asteroids as bodies like planets:
    - Insert them into the quad tree.
    - Calculate forces on them using the Barnes-Hut method.
  - No special treatment is needed; the algorithm handles them automatically.

#### 7. Monitor Gravitational Forces and Movements
- **Purpose**: Track asteroid behavior for analysis or visualization.
- **Steps**:
  - For each asteroid, at each time step:
    - Log or store:
      - Position (x, y).
      - Velocity (vx, vy).
      - Total force vector (from step 4).
    - Optionally compute force magnitude or direction for detailed monitoring.
  - **Visualization**:
    - Render asteroids distinctly (e.g., smaller size, different color) in your rendering function.
    - Optionally draw trajectories (line of past positions) or force vectors for insight.
    - For debugging, consider drawing quad tree boundaries.

#### 8. Optimize and Test
- **Purpose**: Ensure efficiency and correctness.
- **Steps**:
  - Test with a small system (e.g., Sun, one planet, one asteroid) and verify orbits match expectations.
  - Gradually increase the number of asteroids and monitor performance.
  - Optimize:
    - Limit quad tree depth to prevent excessive subdivision.
    - Profile and refine tree construction if it becomes a bottleneck.
  - Adjust \( \theta \) and \( dt \) to balance accuracy and performance.

### Summary
The quad tree with Barnes-Hut makes your simulation scalable and accurate by reducing force calculation complexity from \( O(N^2) \) to \( O(N \log N) \), allowing you to include all gravitational interactions and add many asteroids without performance issues. The plan above guides you through building the quad tree, computing forces, updating the simulation, and monitoring asteroids, enhancing both realism and functionality. Start by implementing the quad tree structure and insertion logic, then proceed step-by-step, testing as you go. This will transform your simulation into a robust model of a dynamic solar system!