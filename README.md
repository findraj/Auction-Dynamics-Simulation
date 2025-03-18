# Modeling and Simulation of Auction Dynamics - SHO

## Project Description

This project focuses on modeling and simulating the dynamics of an auction using the SHO (Simulation of Human Operations) approach. The goal is to understand how different bidding strategies affect the outcome of an auction. The simulation is based on real-world data from online auctions, particularly from eBay, and aims to validate the effectiveness of various bidding strategies.

### Features

- **Auction Simulation**: The project simulates a classic fixed-time auction with increasing bids.
- **Bidding Strategies**: Three main bidding strategies are implemented:
  - **Ratchet Bidding**: Incremental bidding in small steps.
  - **Snipe Bidding**: Bidding at the last moment to secure the item at the lowest possible price.
  - **Agent Bidding**: Automated bidding using software agents.
- **Validation**: The simulation is validated against real-world data from eBay auctions.
- **Statistical Analysis**: The project includes statistical analysis of the outcomes of different bidding strategies.

## Implementation

The project is implemented in C++ using the SIMLIB simulation library. The simulation models the behavior of bidders and the auction process, including the timing of bids and the decision-making process of bidders.

### Key Components

- **Petri Net Model**: The auction process is modeled using a Petri net, which captures the parallel and sequential activities of bidders.
- **Bidder Classes**: Different classes represent bidders using different strategies:
  - **AgentBidder**: Represents bidders using automated agents.
  - **RatchetBidder**: Represents bidders using incremental bidding.
  - **SnipeBidder**: Represents bidders using last-moment bidding.
- **Auction Process**: The auction process is simulated, including the generation of bidders, the timing of bids, and the determination of the winning bid.

## Experiments

Two main experiments were conducted to validate the model:

### Experiment 1: Validation of Bidding Strategies

This experiment aimed to validate the effectiveness of different bidding strategies based on data from a referenced study. The results confirmed that the **Agent Bidding** strategy has the highest probability of winning (approximately 67%), followed by **Ratchet Bidding** (28.1%) and **Snipe Bidding** (4.8%).

### Experiment 2: Comparison with Real-World Auctions

This experiment compared the simulation results with real-world auction data from eBay. The results showed that the simulation accurately captures the behavior of bidders and the dynamics of the auction process.

## Conclusion

The project successfully models and simulates the dynamics of an auction, validating the effectiveness of different bidding strategies. The results confirm that **Agent Bidding** is the most effective strategy for winning auctions, with a high probability of success. The simulation also accurately reflects real-world auction behavior, as demonstrated by the comparison with eBay data.

### Self-Evaluation

The project was evaluated based on the accuracy of the simulation results compared to real-world data and the referenced study. The results confirm the validity of the model and the effectiveness of the implemented bidding strategies.

## References

- Cui, X.; Lai, V. S.; Lowry, P. B.; et al.: *The effects of bidder factors on online bidding strategies: A motivation-opportunity-ability (MOA) model*. Decision Support Systems, 2020.
- Jank, W.; Shmueli, G.: *Modeling Online Auctions*. 2010.
- Peringer, P.; Hrubý, M.: *Modelování a simulace*. 2024.
- Peringer, P.; Leska, D.; Martinek, D.: *SIMulation LIBrary for C++*. 2021.
- Wikipedia contributors: *Auction — Wikipedia, The Free Encyclopedia*. 2024.
