/**
 * @file auction.cpp
 * @brief Auction simulation with multiple bidders
 * Bidders strategies are: Agent-bidding, Ratchet-bidding, and Sniping
 *
 * @authors Marko Olešák (xolesa00), Ján Findra (xfindr01)
 */

#include <iostream>
#include <ctime>
#include <cmath>
#include "simlib.h"
#include <cstdint>
#include <vector>
using namespace std;

#define LOGGING true

// Simulation parameters
const double NUMBER_OF_ITEMS = 1000;      // Number of auction items
const double NUMBER_OF_BIDDERS = 70;      // Number of potential bidders for each item
const double SINGLE_ITEM_DURATION = 60.0; // Duration of a single auction item

// Simulation helping variables
double currentPrice = -1;                                 // Current price of the auction
double minimalIncrement() { return currentPrice * 0.01; } // Current increment of the auction
bool firstBidPlaced = false;                              // Flag if the first bid was placed for an item
double ItemEndTime = 0;                                   // End time of the current item

enum BidderType
{
    AGENT,
    RATCHET,
    SNIPER,
    NONE = -1
};

// Statistics
uint32_t itemNumber = 0; // Unique identifier of the item
int lastBidder = NONE;   // Helper variable for histogram

Facility biddingFacility("Bidding process");         // Facility for bidding
Facility runningAuction("Item auction");             // Facility for running the auction
Histogram winners("Winners", -1, 1, 4);              // Histogram of winners
Queue AgentDecidedToBid("Agent decided to bid");     // Queue of agents that decided to bid
Queue RatchetDecidedToBid("Ratchet decided to bid"); // Queue of ratchet bidders that decided to bid
Queue SniperDecidedToBid("Sniper decided to bid");   // Queue of snipers that decided to bid
Process *AgentBidsProcess;                           // Agent bids handler
Process *RatchetBidsProcess;                         // Ratchet bids handler
Process *SniperBidsProcess;                          // Sniper bids handler

/**
 * @brief Logs a single bid to a file
 * Function is used for further analysis of the auction
 *
 * @param bidAmount Amount of the bid
 *
 * @return void
 */
void logSingleBid(double bidAmount)
{
    static bool header = false; // Variable to check if the header was already written
    FILE *logFile = fopen("analysis/results/auction_detailed_log.csv", "a");
    if (logFile)
    {
        if (!header)
        {
            header = true;
            fprintf(logFile, "ItemNumber,ItemTime,BidAmount\n"); // Header
        }

        // Time since the start of the auction for the current item
        double itemTime = SINGLE_ITEM_DURATION - (ItemEndTime - Time);
        // Log the bid
        fprintf(logFile, "%d,%.1f,%.2f\n", itemNumber, itemTime, bidAmount);
        fclose(logFile);
    }
}

/**
 * @brief Funcion gets the first bidder from the queues and activates it
 *
 * @return void
 */
void returnFromQueues()
{
    while (!AgentDecidedToBid.Empty())
    {
        AgentDecidedToBid.GetFirst()->Activate();
    }
    while (!RatchetDecidedToBid.Empty())
    {
        RatchetDecidedToBid.GetFirst()->Activate();
    }
    while (!SniperDecidedToBid.Empty())
    {
        SniperDecidedToBid.GetFirst()->Activate();
    }
}

/**
 * @class AgentBidder
 * @brief Represents an agent bidder strategy in an auction.
 *
 * @details
 * Agents bid higher than the current price by the minimum increment if the current price is lower than the agent's item valuation.
 * The bidding behavior is influenced by patience, which decreases over time.
 *
 * @note The agent does not engage in bidding during the early stages of the auction.
 *
 * @param valuation The maximum price the agent is willing to pay for the item.
 * @param roundEndTime The time at which the auction round ends.
 */
class AgentBidder : public Process
{
private:
    // Behaviour helpers
    double lastUpdateTime = 0;
    const double UPDATE_INTERVAL = SINGLE_ITEM_DURATION / 100;

    double valuation;
    bool isLeading = false;
    double patience = 1.0;
    double roundEndTime = 0;
    double aliveTime = 0;

public:
    /**
     * @brief Constructs an AgentBidder with a specified valuation and round end time.
     * @param val The maximum price the agent is willing to pay for the item.
     * @param roundEndTime The time at which the auction round ends.
     */
    AgentBidder(double val, double roundEndTime) : valuation(val)
    {
        this->roundEndTime = roundEndTime;
    }

    /**
     * @brief The behavior of the agent bidder.
     */
    void Behavior()
    {
        while ((currentPrice < this->valuation) && (this->patience > Exponential(0.1)) && (Time < this->roundEndTime))
        {
            // Check if enough time has passed since the last update
            if ((Time - lastUpdateTime) >= UPDATE_INTERVAL)
            {
                updatePatience();
                lastUpdateTime = Time; // Update the timestamp
            }

            Wait(max(this->patience, 0.2));
            aliveTime += max(this->patience, 0.2);

            // Agents do not engage in bidding in the early stages of the auction
            if (Time > (this->roundEndTime - (Exponential((SINGLE_ITEM_DURATION / 4) * 3))))
            {
                if ((Random() > this->patience) && ((currentPrice + minimalIncrement()) < this->valuation))
                {
                    printf("[AGENT] bidder decided to bid with alive time %.2f\n", aliveTime);
                    Wait(0.5);
                    if (Time >= this->roundEndTime)
                    {
                        Terminate();
                    }
                    AgentDecidedToBid.Insert(this);
                    Passivate();
                }
            }
        }
        // Stop if patience is exhausted
        if (this->patience <= 0)
        {
            printf("[AGENT] bidder ran out of patience and stopped bidding.\n");
        }
        Terminate();
    }

    /**
     * @brief Updates the patience of the agent bidder based on the time remaining in the auction of an item.
     */
    void updatePatience()
    {
        double normalizedTime = (SINGLE_ITEM_DURATION - (ItemEndTime - Time)) / SINGLE_ITEM_DURATION;

        if (normalizedTime < 0.75)
        {
            this->patience = 1.0 - (Exponential(0.01));
        }
        else
        {
            double remainingTime = (normalizedTime - 0.75) / (1.0 - 0.75);
            this->patience = 0.99 - 0.1 * pow(remainingTime, 5);
        }
    }
};

/**
 * @class AgentBids
 * @brief Represents the bidding process of agents in an auction.
 */
class AgentBids : public Process
{
    void Behavior()
    {
        while (Time < ItemEndTime)
        {
            Wait(0.1); // Time to process the bid

            if (Time >= ItemEndTime)
            {
                Passivate();
            }
            if (!AgentDecidedToBid.Empty())
            {
                if (!biddingFacility.Busy())
                {
                    Seize(biddingFacility);
                    firstBidPlaced = true;
                    currentPrice += minimalIncrement();
                    if (LOGGING)
                    {
                        logSingleBid(currentPrice);
                    }
                    printf("[AGENT] bidder placed a bid at time: %.2f. New price: %.2f\n", Time, currentPrice);
                    lastBidder = AGENT;
                    returnFromQueues();
                    Release(biddingFacility);
                }
            }
        }
        Passivate();
    }
};

/**
 * @class RatchetBidder
 * @brief Represents a ratchet bidder strategy in an auction.
 *
 * @details
 * Ratchet bidders bid higher than the current price by the minimum increment if the current price is lower than the agent's item valuation.
 * The bidding behavior is influenced by patience, which decreases over time.
 *
 * @note Ratchet bidders are humans, sometimes they are irrational and bid with a unrealistic price valuation.
 *
 * @param valuation The maximum price the ratchet bidder is willing to pay for the item.
 * @param roundEndTime The time at which the auction round ends.
 */
class RatchetBidder : public Process
{
private:
    // Behaviour helpers
    const double UPDATE_INTERVAL = SINGLE_ITEM_DURATION / 100;
    double lastUpdateTime = 0;

    double valuation = 0;    // The maximum price the agent is willing to pay for the item
    bool isLeading = false;  // Whether the bidder is leading the auction
    double patience = 1.0;   // Initial patience
    double aliveTime = 0;    // TODO
    double roundEndTime = 0; // Prevents bidding after the end of the auction round

public:
    /**
     * @brief Constructs a RatchetBidder with a specified valuation and round end time.
     * @param val The maximum price the agent is willing to pay for the item.
     * @param roundEndTime The time at which the auction round for an item ends.
     */
    RatchetBidder(double val, double roundEndTime) : valuation(val)
    {
        this->roundEndTime = roundEndTime;

        // 2% chance of being irrational
        if (Random() < 0.02)
        {
            this->valuation = INFINITY;
        }
    }

    /**
     * @brief The behavior of the ratchet bidder.
     */
    void Behavior()
    {
        while ((currentPrice < this->valuation) && (this->patience > Exponential(0.1)) && (Time < this->roundEndTime))
        {
            if ((Time - lastUpdateTime) >= UPDATE_INTERVAL)
            {
                updatePatience();
                lastUpdateTime = Time;
            }

            Wait(max(this->patience, 0.2));
            aliveTime += max(this->patience, 0.2); // TODO

            // Check if the bidder should bid
            if ((Random() > this->patience) && ((currentPrice + minimalIncrement()) <= valuation))
            {
                printf("[RATCHET] bidder decided to bid with alive time %.2f\n", aliveTime);
                Wait(1);
                if (Time >= this->roundEndTime)
                {
                    Terminate();
                }
                RatchetDecidedToBid.Insert(this);
                Passivate();
            }
        }
        if (this->patience <= 0)
        {
            printf("[RATCHET] ran out of patience and stopped bidding.\n");
        }
        Terminate();
    }

    /**
     * @brief Updates the patience of the ratchet bidder based on the time remaining in the auction of an item.
     */
    void updatePatience()
    {
        double normalizedTime = (SINGLE_ITEM_DURATION - (this->roundEndTime - Time)) / SINGLE_ITEM_DURATION;
        if (normalizedTime < 0.75)
        {
            this->patience = 1.0 - (Exponential(0.01));
        }
        else
        {
            double remainingTime = (normalizedTime - 0.75) / (1.0 - 0.75);
            this->patience = 0.99 - 0.1 * pow(remainingTime, 5);
        }
    }
};

/**
 * @class RatchetBids
 * @brief Represents the bidding process of ratchet bidders in an auction.
 */
class RatchetBids : public Process
{
    void Behavior()
    {
        while (Time < ItemEndTime)
        {
            Wait(0.1); // Time to process the bid
            if (Time >= ItemEndTime)
            {
                Passivate();
            }
            if (!RatchetDecidedToBid.Empty())
            {
                if (!biddingFacility.Busy())
                {
                    Seize(biddingFacility);
                    firstBidPlaced = true;
                    currentPrice += minimalIncrement();
                    if (LOGGING)
                    {
                        logSingleBid(currentPrice);
                    }
                    printf("[RATCHET] bidder placed a bid at time: %.2f. New price: %.2f\n", Time, currentPrice);
                    lastBidder = RATCHET;
                    returnFromQueues();
                    Release(biddingFacility);
                }
            }
        }
        Passivate();
    }
};

/**
 * @class SnipingBidder
 * @brief Represents a sniping bidder strategy in an auction.
 *
 * @details
 * Sniping bidders bid higher than the current price by the minimum increment if the current price is lower than their item valuation.
 * The bidding behavior is influenced by human reaction time and network latency.
 *
 * @note Sniping bidders generally do not want to bid when the price is high and their price valuation is lower.
 *
 * @param valuation The maximum price a sniper is willing to pay for the item.
 * @param roundEndTime The time at which the auction round ends.
 */
class SnipingBidder : public Process
{
private:
    double valuation = 0;
    double snipeDelay = Normal(0, 0.1 / 3);
    double roundEndTime = 0;

public:
    /**
     * @brief Constructs a SnipingBidder with a specified valuation and round end time.
     * @param val The maximum price the sniper is willing to pay for the item.
     * @param roundEndTime The time at which the auction round for an item ends.
     */
    SnipingBidder(double val, double roundEndTime) : valuation(val)
    {
        this->roundEndTime = roundEndTime;
    }

    /**
     * @brief The behavior of the sniping bidder.
     */
    void Behavior()
    {
        // printf("[SNIPER] bidder created with valuation %.2f\n", valuation);
        double snipeTime = this->roundEndTime - this->snipeDelay;
        if (Time < snipeTime)
        {
            Wait(snipeTime - Time);
        }

        Wait(Exponential(0.2)); // Reaction time
        Wait(Exponential(0.1)); // Network latency

        if (Time > this->roundEndTime)
        {
            Terminate();
        }

        if ((currentPrice + minimalIncrement()) <= valuation)
        {
            printf("[SNIPER No. %lu] bidder decided to bid at time: %.2f\n", id(), Time);
            SniperDecidedToBid.Insert(this);
            Passivate();
        }
        Terminate();
    }
};

/**
 * @class SniperBids
 * @brief Represents the bidding process of sniping bidders in an auction.
 */
class SniperBids : public Process
{
    void Behavior()
    {
        while (Time < ItemEndTime)
        {
            Wait(0.1); // Time to process the bid
            if (Time >= ItemEndTime)
            {
                Passivate();
            }
            if (!SniperDecidedToBid.Empty())
            {
                if (!biddingFacility.Busy())
                {
                    Seize(biddingFacility);
                    firstBidPlaced = true;
                    currentPrice += minimalIncrement();
                    if (LOGGING)
                    {
                        logSingleBid(currentPrice);
                    }
                    printf("[SNIPER No. %lu] bidder placed a bid at time: %.2f. New price: %.2f\n", SniperDecidedToBid.GetFirst()->id(), Time, currentPrice);
                    lastBidder = SNIPER;
                    returnFromQueues();
                    Release(biddingFacility);
                }
            }
        }
        Passivate();
    }
};

/**
 * @class BidderGenerator
 * @brief Generates bidders for an auction item.
 *
 * @details
 * The bidder generator creates a specified number of bidders for an auction item.
 *
 * @note The bidder generator generates agents, ratchet bidders, and snipers based on the probabilities of each strategy.
 * The probabilities are set according to the reference paper.
 *
 * @param roundEndTime The time at which the auction round for an item ends.
 * @param realPrice The real price of the item.
 *
 */
class BidderGenerator : public Process
{
private:
    double RoundEndTime = 0;
    double RealPrice = 0;

public:
    /**
     * @brief Constructs a BidderGenerator with a specified round end time and real price.
     * @param roundEndTime The time at which the auction round for an item ends.
     * @param realPrice The real price of the item.
     */
    BidderGenerator(double roundEndTime, double realPrice)
    {
        this->RoundEndTime = roundEndTime;
        this->RealPrice = realPrice;
    }

    /**
     * @brief The behavior of the bidder generator.
     */
    void Behavior()
    {
        int agents = 0;
        int ratchets = 0;
        int snipers = 0;
        int roundBidders = max(Exponential(NUMBER_OF_BIDDERS), 0.0);
        for (int i = 0; i < NUMBER_OF_BIDDERS; i++)
        {
            // Calculate probability of each strategy
            // Agent-bidding: 40%
            // Ratchet-bidding: 25%
            // Sniping: 35%
            // Follows the reference paper
            double probability = Random();

            // Wait between the potential bidders to simulate real auction
            Wait(Exponential((SINGLE_ITEM_DURATION / 2) / NUMBER_OF_BIDDERS));

            // Generate bidder with the given strategy
            if (probability < 0.4)
            {
                Process *agenProc = new AgentBidder(RealPrice * Normal(1.2, 0.5 / 2), this->RoundEndTime);
                agenProc->Activate();
                agents++;
            }
            else if (probability < 0.65)
            {
                Process *ratchetProc = new RatchetBidder(RealPrice * Normal(1.2, 0.5 / 2), this->RoundEndTime);
                ratchetProc->Activate();
                ratchets++;
            }
            else
            {
                // Snipers generally do not want to bid, when the price is high, and their price valuation is lower
                Process *sniperProc = new SnipingBidder(RealPrice * Normal(1.2, 0.3 / 2), this->RoundEndTime);
                sniperProc->Activate();
                snipers++;
            }
        }
        printf("Generated %d agents, %d ratchets, %d snipers\n", agents, ratchets, snipers);
        Terminate();
    }
};

/**
 * @class FirstBidTimeout
 * @brief Represents a timeout for the first bid in an auction.
 *
 * @details
 * The first bid timeout checks if a bid was placed in the first 30 seconds of an auction item.
 * If no bid was placed, the item is discarded.
 *
 * @param p The process of the auction item.
 * @param dt The time after which the timeout occurs.
 * @param firstBidPlaced The flag indicating if the first bid was placed.
 */
class FirstBidTimeout : public Event
{
    Process *id;
    bool *placed;

public:
    FirstBidTimeout(Process *p, double dt, bool *firstBidPlaced) : id(p)
    {
        placed = firstBidPlaced;
        Activate(Time + dt);
    }

    void Behavior()
    {
        if (!*placed)
        {
            printf("No bids were placed in the first 30 seconds, the item is discarded\n");
            // id->Release(runningAuction); // TODO
            id->Cancel();
            winners(NONE);
        }
        Cancel();
    }
};

/**
 * @class AuctionItem
 * @brief Represents an auction item.
 *
 * @details
 * The auction item generates bidders for the item and handles the auction process.
 *
 * @note The auction item is discarded if no bid is placed in the first 30 seconds.
 */
class AuctionItem : public Process
{
public:
    bool isSold = false;
    void Behavior()
    {
        Priority = 10;

        // Set the end time of the item
        ItemEndTime = Time + SINGLE_ITEM_DURATION;
        itemNumber++;

        // Generate the value of the item
        double RealPrice = Exponential(1000 * Normal(1.0, 0.2));
        printf("Created item with value %.2f\n", RealPrice);

        // Reset the last bidder
        lastBidder = NONE;

        // Starting price of the item
        currentPrice = RealPrice * Normal(0.8, 0.2);

        // Reset the current price
        printf("Auction started for item valued at %.2f\n", currentPrice);

        AgentBidsProcess = new AgentBids();
        RatchetBidsProcess = new RatchetBids();
        SniperBidsProcess = new SniperBids();
        AgentBidsProcess->Activate();
        RatchetBidsProcess->Activate();
        SniperBidsProcess->Activate();

        // Create bidders
        (new BidderGenerator(ItemEndTime, RealPrice))->Activate();

        // If there are no bidders in the first 30 seconds, the item is discarded
        FirstBidTimeout *firstBidTimeout = new FirstBidTimeout(this, 30, &firstBidPlaced);

        printf("This auction will end at %.2f\n", ItemEndTime);
        printf("Current time is %.2f\n", Time);

        // Wait until the end of the auction
        Wait(SINGLE_ITEM_DURATION);
        printf("Auction ended\n");

        // If a bid was placed, the item is sold
        if (firstBidPlaced)
        {
            isSold = true;
            printf("Item sold at price %.2f\n", currentPrice);
            printf("Winner: %d\n", lastBidder);
            winners(lastBidder);
        }
        else
        {
            // Should not happen, it is caught by the timeout
            printf("Item not sold (no bids)\n");
        }

        // Terminate the bids processes
        AgentBidsProcess->Terminate();
        RatchetBidsProcess->Terminate();
        SniperBidsProcess->Terminate();
        delete firstBidTimeout;
        Terminate();
    }
};

/**
 * @class Auction
 * @brief Represents an auction process for a set of auction items.
 */
class Auction : public Process
{
public:
    /**
     * @brief The behavior of the auction process.
     */
    void Behavior()
    {
        while (itemNumber < NUMBER_OF_ITEMS)
        {
            // Indicates the end of the auction for a single item
            Seize(runningAuction);
            printf("AUCTION STARTED\n");

            // Create and activate a new auction item
            AuctionItem *item = new AuctionItem();
            item->Activate();

            returnFromQueues();
            // Pause between items
            Wait(SINGLE_ITEM_DURATION + 30);

            Release(runningAuction);
        }
        printf("All items auctioned!\n");
    }
};

/**
 * @brief Main function of the simulation.
 */
int main()
{
    // Set a random seed
    RandomSeed(time(NULL));

    // The simulation time
    Init(0, (SINGLE_ITEM_DURATION + 30) * NUMBER_OF_ITEMS); // Single item duration + 30 seconds between items

    // Run the simulation
    (new Auction)->Activate();
    Run();

    printf("Simulation finished\n");

    // Statistics
    SetOutput("stats.out");
    biddingFacility.Output();
    winners.Output();
    runningAuction.Output();
}