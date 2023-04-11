/**
 * @file agent_group.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-11
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

namespace contention_prof {

template <typename Agent>
class AgentGroup {
public:
    using agent_type = Agent;
    const static size_t RAW_BLOCK_SIZE = 4096;
    const static size_t ELEMENT_PER_BLOCK = (RAW_BLOCK_SIZE + sizeof(Agent) - 1) / sizeof(Agent);

    struct ThreadBlock {
        inline Agent* at(size_t offset) {
            return agents_ + offset;
        }
    private:
        Agent agents_[ELEMENT_PER_BLOCK];
    };

    inline static int create_new_agent() {
        
    }

};

}  // namespace contention_prof
