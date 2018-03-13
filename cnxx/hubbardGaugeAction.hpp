#ifndef HUBBARD_GAUGE_ACTION_HPP
#define HUBBARD_GAUGE_ACTION_HPP

#include "action.hpp"

namespace cnxx {
    /// Pure gauge action for Hubbard model.
    /**
     * The action is
     \f[
     S_{\mathrm{HGA}} = \frac{1}{2\tilde{U}} \textstyle\sum_{x,t}\,\phi^2_{xt}.
     \f]
     */
    struct HubbardGaugeAction : Action {
        double utilde;  ///< Parameter \f$\tilde{U}\f$.

        /// Set \f$\tilde{U}\f$.
        explicit HubbardGaugeAction(const double utilde_) : utilde(utilde_) { }

        ~HubbardGaugeAction() override = default;

        /// Evaluate the %Action for given auxilliary field phi.
        std::complex<double> eval(const Vector<std::complex<double>> &phi) override;

        /// Calculate force for given auxilliary field phi.
        Vector<std::complex<double>> force(const Vector<std::complex<double>> &phi) override;

        /// Evaluate %Action and compute force for given auxilliary field phi.
        std::pair<std::complex<double>, Vector<std::complex<double>>> valForce(
            const Vector<std::complex<double>> &phi) override;
    };
}  // namespace cnxx

#endif  // ndef HUBBARD_GAUGE_ACTION_HPP