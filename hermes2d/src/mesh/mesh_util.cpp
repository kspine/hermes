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

#include "mesh.h"
#include "refmap.h"

namespace Hermes
{
  namespace Hermes2D
  {
    void MeshUtil::assign_nurbs(Node* en, Nurbs* nurbs, int p1, int p2)
    {
      // assign the arc to the elements sharing the edge node
      for (unsigned int node_i = 0; node_i < 2; node_i++)
      {
        Element* e = en->elem[node_i];
        if (e == nullptr)
          continue;

        if (e->cm == nullptr)
        {
          e->cm = new CurvMap;
          memset(e->cm, 0, sizeof(CurvMap));
          e->cm->toplevel = 1;
          e->cm->order = 4;
        }

        int idx = -1;
        for (unsigned j = 0; j < e->get_nvert(); j++)
        {
          if (e->en[j] == en)
          {
            idx = j;
            break;
          }
        }
        assert(idx >= 0);

        if (e->vn[idx]->id == p1)
        {
          e->cm->nurbs[idx] = nurbs;
          nurbs->ref++;
        }
        else
        {
          Nurbs* nurbs_rev = MeshUtil::reverse_nurbs(nurbs);
          e->cm->nurbs[idx] = nurbs_rev;
          nurbs_rev->ref++;
        }
      }
      if (!nurbs->ref) delete nurbs;
    }

    Nurbs* MeshUtil::reverse_nurbs(Nurbs* nurbs)
    {
      Nurbs* rev = new Nurbs;
      *rev = *nurbs;
      rev->twin = true;

      rev->pt = new double3[nurbs->np];
      for (int i = 0; i < nurbs->np; i++)
      {
        rev->pt[nurbs->np - 1 - i][0] = nurbs->pt[i][0];
        rev->pt[nurbs->np - 1 - i][1] = nurbs->pt[i][1];
        rev->pt[nurbs->np - 1 - i][2] = nurbs->pt[i][2];
      }

      rev->kv = new double[nurbs->nk];
      for (int i = 0; i < nurbs->nk; i++)
        rev->kv[i] = nurbs->kv[i];
      for (int i = nurbs->degree + 1; i < nurbs->nk - nurbs->degree - 1; i++)
        rev->kv[nurbs->nk - 1 - i] = 1.0 - nurbs->kv[i];

      rev->arc = nurbs->arc;
      rev->angle = -nurbs->angle;
      return rev;
    }

    Node* MeshUtil::get_base_edge_node(Element* base, int edge)
    {
      while (!base->active) // we need to go down to an active element
      {
        int son1, son2;
        get_edge_sons(base, edge, son1, son2);
        base = base->sons[son1];
      }
      return base->en[edge];
    }

    int MeshUtil::get_edge_sons(Element* e, int edge, int& son1, int& son2)
    {
      assert(!e->active);

      if (!e->is_triangle())
      {
        if (e->sons[2] == nullptr) // horz quad
        {
          if (edge == 0 || edge == 2) { son1 = edge >> 1;   return 1; }
          else if (edge == 1) { son1 = 0; son2 = 1; return 2; }
          else { son1 = 1; son2 = 0; return 2; }
        }
        else if (e->sons[0] == nullptr) // vert quad
        {
          if (edge == 1 || edge == 3) { son1 = (edge == 1) ? 3 : 2; return 1; }
          else if (edge == 0) { son1 = 2; son2 = 3; return 2; }
          else { son1 = 3; son2 = 2; return 2; }
        }
      }

      // triangle or 4-son quad
      son1 = edge;
      son2 = e->next_vert(edge);
      return 2;
    }

    MeshHashGrid::MeshHashGrid(Mesh* mesh) : mesh_seq(mesh->get_seq())
    {
      mesh->calc_bounding_box();

      // create grid
      double interval_len_x = (mesh->top_right_x - mesh->bottom_left_x) / GRID_SIZE;
      double interval_len_y = (mesh->top_right_y - mesh->bottom_left_y) / GRID_SIZE;

      intervals_x[0] = mesh->bottom_left_x;
      intervals_y[0] = mesh->bottom_left_y;

      for (int i = 1; i < GRID_SIZE; i++)
      {
        intervals_x[i] = intervals_x[i - 1] + interval_len_x;
        intervals_y[i] = intervals_y[i - 1] + interval_len_y;
      }

      intervals_x[GRID_SIZE] = mesh->top_right_x;
      intervals_y[GRID_SIZE] = mesh->top_right_y;

      for (int i = 0; i < GRID_SIZE; i++)
      {
        for (int j = 0; j < GRID_SIZE; j++)
        {
          m_grid[i][j] = new MeshHashGridElement(intervals_x[i], intervals_y[j], intervals_x[i + 1], intervals_y[j + 1]);
        }
      }

      // assign elements
      Element *element;
      double2 p1, p2;
      int x_min, x_max, y_min, y_max;
      for_all_active_elements(element, mesh)
      {
        elementBoundingBox(element, p1, p2);

        x_min = 0;
        while (intervals_x[x_min + 1] < p1[0])
          x_min++;

        x_max = GRID_SIZE - 1;
        while (intervals_x[x_max] > p2[0])
          x_max--;

        y_min = 0;
        while (intervals_y[y_min + 1] < p1[1])
          y_min++;

        y_max = GRID_SIZE - 1;
        while (intervals_y[y_max] > p2[1])
          y_max--;

        assert((x_max >= x_min) && (y_max >= y_min));

        for (int i = x_min; i <= x_max; i++)
        {
          for (int j = y_min; j <= y_max; j++)
          {
            assert(m_grid[i][j]->belongs(element));
            m_grid[i][j]->insert(element);
          }
        }
      }
    }

    MeshHashGrid::~MeshHashGrid()
    {
      for (int i = 0; i < GRID_SIZE; i++)
      {
        for (int j = 0; j < GRID_SIZE; j++)
        {
          delete m_grid[i][j];
        }
      }
    }

    MeshHashGridElement::MeshHashGridElement(double lower_left_x, double lower_left_y, double upper_right_x, double upper_right_y, int depth) : lower_left_x(lower_left_x), lower_left_y(lower_left_y), upper_right_x(upper_right_x), upper_right_y(upper_right_y), m_depth(depth), m_active(true), element_count(0)
    {
      this->elements = new Element*[MAX_ELEMENTS];
      for (int i = 0; i < 2; i++)
      for (int j = 0; j < 2; j++)
        m_sons[i][j] = nullptr;
    }

    MeshHashGridElement::~MeshHashGridElement()
    {
      if (elements)
        delete[] elements;
      for (int i = 0; i < 2; i++)
      for (int j = 0; j < 2; j++)
      if (m_sons[i][j])
        delete m_sons[i][j];
    }

    bool MeshHashGridElement::belongs(Element *element)
    {
      double2 p1, p2;
      MeshHashGrid::elementBoundingBox(element, p1, p2);
      return ((p1[0] <= upper_right_x) && (p2[0] >= lower_left_x) && (p1[1] <= upper_right_y) && (p2[1] >= lower_left_y));
    }

    void MeshHashGridElement::insert(Element *element)
    {
      if (m_active)
      {
        elements[element_count++] = element;

        if ((element_count >= MAX_ELEMENTS) && (m_depth < MAX_DEPTH))
        {
          m_active = false;
          double xx[3] = { lower_left_x, (lower_left_x + upper_right_x) / 2., upper_right_x };
          double yy[3] = { lower_left_y, (lower_left_y + upper_right_y) / 2., upper_right_y };
          for (int i = 0; i < 2; i++)
          {
            double x0 = xx[i];
            double x1 = xx[i + 1];
            for (int j = 0; j < 2; j++)
            {
              double y0 = yy[j];
              double y1 = yy[j + 1];

              assert(m_sons[i][j] == nullptr);

              m_sons[i][j] = new MeshHashGridElement(x0, y0, x1, y1, m_depth + 1);

              for (int elem_i = 0; elem_i < this->element_count; elem_i++)
              {
                if (m_sons[i][j]->belongs(elements[elem_i]))
                  m_sons[i][j]->insert(elements[elem_i]);
              }
            }
          }

          delete[] elements;
          elements = nullptr;
        }
      }
      else
      {
        for (int i = 0; i < 2; i++)
        {
          for (int j = 0; j < 2; j++)
          {
            assert(m_sons[i][j]);
            if (m_sons[i][j]->belongs(element))
              m_sons[i][j]->insert(element);
          }
        }
      }
    }

    bool MeshHashGridElement::belongs(double x, double y)
    {
      return (x >= lower_left_x) && (x <= upper_right_x) && (y <= lower_left_y) && (y >= upper_right_y);
    }

    Element* MeshHashGridElement::getElement(double x, double y)
    {
      if (m_active)
      {
        for (int elem_i = 0; elem_i < this->element_count; elem_i++)
        if (RefMap::is_element_on_physical_coordinates(elements[elem_i], x, y))
          return elements[elem_i];

        return nullptr;
      }
      else
      {
        Element* element;
        for (int i = 0; i < 2; i++)
        {
          for (int j = 0; j < 2; j++)
          {
            element = m_sons[i][j]->getElement(x, y);
            if (element)
              return element;
          }
        }
        return nullptr;
      }
    }

    void MeshHashGrid::elementBoundingBox(Element *element, double2 &p1, double2 &p2)
    {
      p1[0] = p2[0] = element->vn[0]->x;
      p1[1] = p2[1] = element->vn[0]->y;

      for (int i = 1; i < element->get_nvert(); i++)
      {
        double xx = element->vn[i]->x;
        double yy = element->vn[i]->y;
        if (xx > p2[0])
          p2[0] = xx;
        if (xx < p1[0])
          p1[0] = xx;
        if (yy > p2[1])
          p2[1] = yy;
        if (yy < p1[1])
          p1[1] = yy;
      }

      if (element->is_curved())
      {
        // todo: should be improved
        double diameter_x = p2[0] - p1[0];
        p2[0] += diameter_x;
        p1[0] -= diameter_x;

        double diameter_y = p2[1] - p1[1];
        p2[1] += diameter_y;
        p1[1] -= diameter_y;
      }
    }

    Element* MeshHashGrid::getElement(double x, double y)
    {
      int i = 0;
      while ((i < GRID_SIZE) && (intervals_x[i + 1] < x))
        i++;

      int j = 0;
      while ((j < GRID_SIZE) && (intervals_y[j + 1] < y))
        j++;

      // this means that x or y is outside mesh, but it can hapen
      if ((i >= GRID_SIZE) || (j >= GRID_SIZE))
        return nullptr;
      else
        return m_grid[i][j]->getElement(x, y);
    }

    int MeshHashGrid::get_mesh_seq() const
    {
      return this->mesh_seq;
    }

    MarkerArea::MarkerArea(Mesh *mesh, int marker) : mesh_seq(mesh->get_seq())
    {
      area = 0;
      Element* elem;
      for_all_active_elements(elem, mesh)
      {
        if (elem->marker == marker)
        {
          area += elem->get_area(true);
        }
      }
    }

    int MarkerArea::get_mesh_seq() const
    {
      return this->mesh_seq;
    }

    double MarkerArea::get_area() const
    {
      return this->area;
    }
  }
}
