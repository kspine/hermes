// This file is part of Hermes2D.
//
// Hermes2D is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// Hermes2D is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Hermes2D.  If not, see <http://www.gnu.org/licenses/>.

#ifndef __H2D_ERROR_CALCULATOR_H
#define __H2D_ERROR_CALCULATOR_H

#include "../weakform/weakform.h"
#include "norm_form.h"
#include "error_thread_calculator.h"

namespace Hermes
{
  namespace Hermes2D
  {
    template<typename Scalar> class ErrorThreadCalculator;

    /// Evaluation of an error between a (coarse) solution and a reference solution. \ingroup g_adapt
    /** The class calculates error estimates and it acts as a container for the calculated errors.
    */
    template<typename Scalar>
    class HERMES_API ErrorCalculator :
      public Hermes::Mixins::TimeMeasurable,
      public Hermes::Mixins::Loggable,
      public Hermes::Hermes2D::Mixins::Parallel
    {
    public:
      enum ErrorCalculationStrategy
      {
        AbsoluteError,
        RelativeError
      };

      /// Constructor. Suitable for problems where various solution components belong to different spaces (L2, H1, Hcurl,
      /// Hdiv). If proj_norms are not specified, they are defined according to the spaces.
      ErrorCalculator(typename ErrorCalculator<Scalar>::ErrorCalculationStrategy strategy);
      
      /// Calculates the errors between coarse_solutions and fine_solutions.
      /// \param[in] sort_and_store If true, these errors are going to be sorted, stored and used for the purposes of adaptivity.
      /// IMPORTANT: if the parameter is passed as false, this, and also any previous error calculations are lost and it is not possible to get back to them.
      void calculate_errors(Hermes::vector<MeshFunctionSharedPtr<Scalar> >& coarse_solutions, Hermes::vector<MeshFunctionSharedPtr<Scalar> >& fine_solutions, bool sort_and_store = true);
      
      /// Calculates the errors between coarse_solutions and fine_solutions.
      /// \param[in] sort_and_store If true, these errors are going to be sorted, stored and used for the purposes of adaptivity.
      void calculate_errors(MeshFunctionSharedPtr<Scalar>& coarse_solution, MeshFunctionSharedPtr<Scalar>& fine_solution, bool sort_and_store = true);

      virtual ~ErrorCalculator();  ///< Destructor. Deallocates allocated private data.

      /// Adds user defined norm form which is used to calculate error.
      /// If the strategy is ErrorCalculationStrategy::RelativeError, this form is also used as a "norm" form to divide the absolute error by the norm of the "fine" solution(s).
      void add_error_form_vol(ContinuousNormForm<Scalar>* form, std::string marker = HERMES_ANY);
      void add_error_form_surf(ContinuousNormForm<Scalar>* form, std::string marker = HERMES_ANY);
      void add_error_form_DG(DiscontinuousNormForm<Scalar>* form);

      /// Returns a squared error of an element.
      /** \param[in] A component index.
      *  \param[in] An element index.
      *  \return Squared error. Meaning of the error depends on parameters of the function calc_errors_internal(). */
      double get_element_error_squared(int component, int element_id) const;
      double get_element_norm_squared(int component, int element_id) const;
      double get_error_squared(int component) const;
      double get_norm_squared(int component) const;
      double get_total_error_squared() const;
      double get_total_norm_squared() const;

    protected:
      /// Initialize the data storage.
      void init_data_storage();

      /// Data.
      Hermes::vector<MeshFunctionSharedPtr<Scalar> > coarse_solutions;
      Hermes::vector<MeshFunctionSharedPtr<Scalar> > fine_solutions;
      
      /// Absolute / Relative error.
      typename ErrorCalculator<Scalar>::ErrorCalculationStrategy strategy;

      /// A reference to an element.
      struct ElementReference {
        int element_id; ///< An element ID. Invalid if below 0.
        int comp; ///< A component which this element belongs to. Invalid if below 0.
        double* error;///< Pointer to the final error, respecting the strategy.
        ElementReference(int element_id, int comp, double* error) : element_id(element_id), comp(comp), error(error) {}; ///< Constructor. It creates an invalid element reference.
      };

      /// A queue of elements which should be processes. The queue had to be filled by the method fill_regular_queue().
      ElementReference** element_references;

      /// Number of solution components.
      int component_count;

      /// A total number of active elements across all provided meshes.
      int num_act_elems;

      /// Errors of elements. Meaning of the error depeds on the strategy.
      double* errors[H2D_MAX_COMPONENTS];
      double* norms[H2D_MAX_COMPONENTS];
      double component_errors[H2D_MAX_COMPONENTS];
      double component_norms[H2D_MAX_COMPONENTS];
      int element_count[H2D_MAX_COMPONENTS];
      double  errors_squared_sum;
      double  norms_squared_sum;

      /// Holds volumetric matrix forms.
      Hermes::vector<ContinuousNormForm<Scalar> *> mfvol;
      /// Holds surface matrix forms.
      Hermes::vector<ContinuousNormForm<Scalar> *> mfsurf;
      /// Holds DG matrix forms.
      Hermes::vector<DiscontinuousNormForm<Scalar> *> mfDG;

      /// This is for adaptivity, saying that the errors are the correct ones.
      bool elements_stored;

      static int compareElementReference (const void * a, const void * b)
      {
        ElementReference* ref_a = (ElementReference*)a;
        ElementReference* ref_b = (ElementReference*)b;
        if(*(ref_a->error) < *(ref_b->error))
          return -1;
        else
          return 1;
      };

      friend class Adapt<Scalar>;
      friend class ErrorThreadCalculator<Scalar>;
    };

    template<typename Scalar, NormType normType>
    class HERMES_API DefaultErrorCalculator : public ErrorCalculator<Scalar>
    {
    public:
      DefaultErrorCalculator(typename ErrorCalculator<Scalar>::ErrorCalculationStrategy strategy, int component_count);
      virtual ~DefaultErrorCalculator();
    };

    template<typename Scalar, NormType normType>
    class HERMES_API DefaultNormCalculator : protected ErrorCalculator<Scalar>
    {
    public:
      DefaultNormCalculator(int component_count);
      virtual ~DefaultNormCalculator();
      
      /// Norms calculation.
      double calculate_norms(Hermes::vector<MeshFunctionSharedPtr<Scalar> >& solutions);
      /// Norms calculation.
      double calculate_norm(MeshFunctionSharedPtr<Scalar> & solution);
    };
  }
}
#endif