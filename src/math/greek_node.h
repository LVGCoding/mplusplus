#pragma once

#include "editor_node.h"


#include <memory>
#include <vector>
#include <string>



class GreekNode : EditorNode{

public:

    ~GreekNode() {};

    void Render(float fontSize) override;

    void KeyPress() override;
    
    void MouseClick() override;
    
    void ToText() override;
    
    std::pair<float,float> GetNWCorner() override;
    
    std::pair<float,float> GetSize() override;
    
    bool HasChild() override;
    
    enum GreekSymbolType{
        alpha,
        beta,
        gamma,
        delta,
        epsilon,
        zeta, 
        eta,
        theta,
        iota,
        kappa,
        lambda,
        mu,
        nu,
        xi,
        omicron,
        pi,
        rho,
        sigma,
        tau,
        upsilon,
        phi,
        chi,
        psi,
        omega
    };
    GreekNode(GreekSymbolType symbol) {this->symbol=symbol;};

private:
    GreekNode::GreekSymbolType symbol;

public:
    GreekSymbolType GetSymbol();

};